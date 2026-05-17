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
        compute_units=compute_units,
        memory_connections=memory,
        clocks=clocks,
        platform=platform_tag,
        target=target,
    )
