"""Parse Vitis v++ link-stage artifacts."""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

from hlsflow.parse_vitis import VitisLogReport, parse_vpp_log


@dataclass(frozen=True)
class ComputeUnit:
    kernel: str
    count: int
    name: str


@dataclass(frozen=True)
class MemoryConnection:
    endpoint: str
    memory: str


@dataclass(frozen=True)
class ClockSetting:
    endpoint: str
    freq_hz: int


@dataclass
class LinkReport:
    build_dir: Path
    xclbins: list[Path] = field(default_factory=list)
    logs: list[VitisLogReport] = field(default_factory=list)
    config_files: list[Path] = field(default_factory=list)
    compute_units: list[ComputeUnit] = field(default_factory=list)
    memory_connections: list[MemoryConnection] = field(default_factory=list)
    clocks: list[ClockSetting] = field(default_factory=list)
    report_files: list[Path] = field(default_factory=list)
    vivado_utilization: dict[str, dict[str, dict[str, float | int]]] = field(default_factory=dict)
    vivado_resource_reports: dict[str, dict[str, object]] = field(default_factory=dict)
    vivado_timing: dict[str, float | bool] = field(default_factory=dict)
    platform: str | None = None
    target: str | None = None

    @property
    def status(self) -> str:
        if any(log.errors for log in self.logs):
            return "fail"
        return "pass" if self.xclbins else "unknown"

    @property
    def errors(self) -> list[str]:
        return [err for log in self.logs for err in log.errors]

    @property
    def warnings(self) -> list[str]:
        return [warn for log in self.logs for warn in log.warnings]


_NK_RE = re.compile(r"^\s*nk\s*=\s*([^:]+)\s*:\s*(\d+)\s*:\s*([^\s#]+)", re.IGNORECASE)
_SP_RE = re.compile(r"^\s*sp\s*=\s*([^:]+)\s*:\s*([^\s#]+)", re.IGNORECASE)
_CLOCK_RE = re.compile(r"^\s*freqHz\s*=\s*(\d+)\s*:\s*([^\s#]+)", re.IGNORECASE)
_PLATFORM_RE = re.compile(r"(?:--platform|platform)\s*(?:=|:)\s*([^\s]+)", re.IGNORECASE)
_TARGET_RE = re.compile(r"(?:--target|target)\s*(?:=|:|\s)\s*(hw_emu|hw|sw_emu)\b", re.IGNORECASE)
_UTIL_CELL_RE = re.compile(r"(?P<used>[0-9,]+)\s*\[\s*(?P<pct>[0-9.]+)%\]")
_TIMING_HEADER_RE = re.compile(r"^\s*WNS\(ns\)\s+TNS\(ns\).*WHS\(ns\).*WPWS\(ns\)", re.MULTILINE)
_VIVADO_META_RE = re.compile(r"^\|\s*(?P<key>Tool Version|Date|Host|Command|Design|Device|Design State)\s*:\s*(?P<value>.*?)\s*$", re.MULTILINE)



def _parse_util_cell(cell: str) -> dict[str, float | int] | None:
    m = _UTIL_CELL_RE.search(cell)
    if not m:
        return None
    return {"used": int(m.group("used").replace(",", "")), "pct": float(m.group("pct"))}


def parse_vivado_utilization_report(path: Path) -> dict[str, dict[str, dict[str, float | int]]]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    if "Accelerator Utilization" not in text and "report_accelerator_utilization" not in text:
        return {}
    out: dict[str, dict[str, dict[str, float | int]]] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 7 or cells[0] in {"Name", ""}:
            continue
        row: dict[str, dict[str, float | int]] = {}
        for key, cell in zip(("LUT", "LUTAsMem", "REG", "BRAM", "URAM", "DSP"), cells[1:7]):
            parsed = _parse_util_cell(cell)
            if parsed:
                row[key] = parsed
        if row:
            out[cells[0].strip()] = row
    return out


def _num_cell(cell: str) -> int | float | str | None:
    text = cell.strip().replace(",", "")
    if not text:
        return None
    if text.startswith("<"):
        try:
            return float(text[1:])
        except ValueError:
            return text
    try:
        return int(text) if "." not in text else float(text)
    except ValueError:
        return text


def parse_vivado_resource_summary_report(path: Path) -> dict[str, object]:
    """Extract machine-readable rows from Vivado report_utilization .rpt files.

    This intentionally captures summary rows from both full utilization and SLR
    utilization reports.  Accelerator-specific kernel-util reports are handled
    by parse_vivado_utilization_report().
    """
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    if "report_utilization" not in text and "Utilization Design Information" not in text:
        return {}
    metadata = {m.group("key").lower().replace(" ", "_"): m.group("value").strip()
                for m in _VIVADO_META_RE.finditer(text)}
    rows: dict[str, dict[str, int | float | str]] = {}
    header: list[str] | None = None
    for raw in text.splitlines():
        stripped = raw.strip()
        if not stripped.startswith("|") or set(stripped) <= {"+", "-", "|", " "}:
            continue
        cells = [c.strip() for c in stripped.strip("|").split("|")]
        if not cells or not cells[0]:
            continue
        first = cells[0]
        if first in {"Site Type", "Name", "SLR Index"} or first.startswith("FROM"):
            header = [c.lower().replace("%", "pct").replace("\\", "to").replace("/", "_").replace(" ", "_")
                      for c in cells]
            continue
        if header is None or len(cells) != len(header):
            continue
        if first in {"Total", "Black Boxes", "Instantiated Netlists"} or first.startswith("Total "):
            key = first
        elif any(ch.isalpha() for ch in first):
            key = first.strip()
        else:
            continue
        parsed: dict[str, int | float | str] = {}
        for col, cell in zip(header[1:], cells[1:]):
            value = _num_cell(cell)
            if value is not None:
                parsed[col] = value
        if parsed:
            if key in rows:
                idx = 2
                while f"{key}#{idx}" in rows:
                    idx += 1
                key = f"{key}#{idx}"
            rows[key] = parsed
    return {"metadata": metadata, "rows": rows} if rows or metadata else {}


def parse_vivado_timing_report(path: Path) -> dict[str, float | bool]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    if "Timing Summary Report" not in text:
        return {}
    out: dict[str, float | bool] = {}
    header = _TIMING_HEADER_RE.search(text)
    if header:
        lines = text[header.end():].splitlines()
        for line in lines:
            nums = re.findall(r"[-+]?\d+\.\d+|[-+]?\d+", line)
            if len(nums) >= 12:
                keys = ["WNS", "TNS", "TNS_failing_endpoints", "TNS_total_endpoints",
                        "WHS", "THS", "THS_failing_endpoints", "THS_total_endpoints",
                        "WPWS", "TPWS", "TPWS_failing_endpoints", "TPWS_total_endpoints"]
                for key, value in zip(keys, nums[:len(keys)]):
                    out[key] = float(value)
                break
    if "All user specified timing constraints are met" in text:
        out["timing_met"] = True
    elif "timing constraints are not met" in text.lower() or "timing failed" in text.lower():
        out["timing_met"] = False
    return out


def _vivado_report_candidates(build_dir: Path) -> list[Path]:
    patterns = ("*util*.rpt", "*timing_summary*.rpt")
    out: list[Path] = []
    for pattern in patterns:
        out.extend(build_dir.rglob(pattern))
    return _dedupe_paths(sorted(out))

def parse_link_config(path: Path) -> tuple[list[ComputeUnit], list[MemoryConnection], list[ClockSetting]]:
    compute_units: list[ComputeUnit] = []
    memory: list[MemoryConnection] = []
    clocks: list[ClockSetting] = []
    if not path.is_file():
        return compute_units, memory, clocks
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if m := _NK_RE.match(line):
            compute_units.append(ComputeUnit(kernel=m.group(1), count=int(m.group(2)), name=m.group(3)))
        elif m := _SP_RE.match(line):
            memory.append(MemoryConnection(endpoint=m.group(1), memory=m.group(2)))
        elif m := _CLOCK_RE.match(line):
            clocks.append(ClockSetting(endpoint=m.group(2), freq_hz=int(m.group(1))))
    return compute_units, memory, clocks


def _dedupe_paths(paths: list[Path]) -> list[Path]:
    seen: set[Path] = set()
    out: list[Path] = []
    for p in paths:
        try:
            key = p.resolve()
        except OSError:
            key = p
        if key not in seen:
            seen.add(key)
            out.append(p)
    return out


def _candidate_log_paths(build_dir: Path) -> list[Path]:
    logs = [p for p in build_dir.rglob("*.log") if _looks_like_link_log(p)]
    return _dedupe_paths(sorted(logs))


def _looks_like_link_log(path: Path) -> bool:
    parts = tuple(part.lower() for part in path.parts)
    if any(part.endswith("_hls") for part in parts):
        return False
    name = path.name.lower()
    joined = "/".join(parts)
    return (
        name.startswith("v++")
        and ("link" in joined or "xclbin" in joined or "run_link" in joined)
    ) or "link" in name


def _extract_log_metadata(logs: list[VitisLogReport]) -> tuple[str | None, str | None]:
    platform = None
    target = None
    for log in logs:
        path = Path(log.log_path)
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")[:20000]
        if platform is None and (m := _PLATFORM_RE.search(text)):
            platform = m.group(1)
        if target is None and (m := _TARGET_RE.search(text)):
            target = m.group(1)
    return platform, target


def _config_candidates(build_dir: Path, platform: str | None) -> list[Path]:
    found = sorted(build_dir.rglob("link.cfg"))
    if not found and platform:
        found.append(Path("config") / platform / "link.cfg")
    return _dedupe_paths([p for p in found if p.is_file()])


def parse_link_artifacts(build_dir: Path, platform: str | None = None) -> LinkReport:
    build_dir = build_dir.resolve()
    xclbins = _dedupe_paths(sorted(build_dir.rglob("*.xclbin")))
    logs = [parse_vpp_log(p) for p in _candidate_log_paths(build_dir)]
    log_platform, target = _extract_log_metadata(logs)
    platform_tag = platform or log_platform
    config_files = _config_candidates(build_dir, platform_tag)
    report_files = _vivado_report_candidates(build_dir)
    vivado_utilization: dict[str, dict[str, dict[str, float | int]]] = {}
    vivado_resource_reports: dict[str, dict[str, object]] = {}
    vivado_timing: dict[str, float | bool] = {}
    for rpt in report_files:
        util = parse_vivado_utilization_report(rpt)
        if util:
            vivado_utilization.update(util)
        resource_summary = parse_vivado_resource_summary_report(rpt)
        if resource_summary:
            vivado_resource_reports[str(rpt.relative_to(build_dir))] = resource_summary
        timing = parse_vivado_timing_report(rpt)
        if timing:
            vivado_timing.update(timing)

    compute_units: list[ComputeUnit] = []
    memory: list[MemoryConnection] = []
    clocks: list[ClockSetting] = []
    for cfg in config_files:
        cu, mem, clk = parse_link_config(cfg)
        compute_units.extend(cu)
        memory.extend(mem)
        clocks.extend(clk)

    return LinkReport(
        build_dir=build_dir,
        xclbins=xclbins,
        logs=logs,
        config_files=config_files,
        report_files=report_files,
        vivado_utilization=vivado_utilization,
        vivado_resource_reports=vivado_resource_reports,
        vivado_timing=vivado_timing,
        compute_units=compute_units,
        memory_connections=memory,
        clocks=clocks,
        platform=platform_tag,
        target=target,
    )
