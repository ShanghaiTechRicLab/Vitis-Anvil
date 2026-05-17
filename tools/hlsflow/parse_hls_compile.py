"""Parse Vitis HLS text compile reports (hls_compile.rpt / *_csynth.rpt)."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass(frozen=True)
class HlsCompileReport:
    report_path: Path
    general: dict[str, str] = field(default_factory=dict)
    m_axi_interfaces: list[dict[str, str]] = field(default_factory=list)
    axilite_registers: list[dict[str, str]] = field(default_factory=list)
    top_arguments: list[dict[str, str]] = field(default_factory=list)
    sw_to_hw_mapping: list[dict[str, str]] = field(default_factory=list)
    burst_summary: list[dict[str, str]] = field(default_factory=list)
    variable_accesses: list[dict[str, str]] = field(default_factory=list)
    burst_status_counts: dict[str, int] = field(default_factory=dict)


_GENERAL_RE = re.compile(r"^\s*\*\s*(?P<key>[^:]+):\s*(?P<value>.*?)\s*$", re.MULTILINE)


def _section(text: str, title: str) -> str:
    marker = f"== {title}"
    start = text.find(marker)
    if start < 0:
        return ""
    next_start = text.find("\n== ", start + len(marker))
    return text[start:] if next_start < 0 else text[start:next_start]


def _rows_after_heading(section: str, heading: str, columns: list[str]) -> list[dict[str, str]]:
    start = section.find(heading)
    if start < 0:
        return []
    lines = section[start:].splitlines()
    rows: list[dict[str, str]] = []
    expected = len(columns)
    for raw in lines:
        stripped = raw.strip()
        if not stripped.startswith("|"):
            if rows:
                break
            continue
        if set(stripped) <= {"+", "-", "|", "=", " "}:
            continue
        cells = [c.strip() for c in stripped.strip("|").split("|")]
        if len(cells) != expected:
            continue
        if any(c in cells[0].lower() for c in ("argument", "interface", "rtl", "hw interface")):
            continue
        if not any(cells):
            continue
        rows.append({key: val for key, val in zip(columns, cells)})
    return rows


def _count_by(rows: list[dict[str, str]], key: str) -> dict[str, int]:
    out: dict[str, int] = {}
    for row in rows:
        value = row.get(key, "").strip() or "unknown"
        out[value] = out.get(value, 0) + 1
    return out


def parse_hls_compile_report(report_path: Path) -> HlsCompileReport:
    text = report_path.read_text(encoding="utf-8", errors="replace")
    general = {m.group("key").strip().lower().replace(" ", "_"): m.group("value").strip()
               for m in _GENERAL_RE.finditer(text)}
    hw = _section(text, "HW Interfaces")
    sw = _section(text, "SW I/O Information")
    burst = _section(text, "M_AXI Burst Information")
    variable_accesses = _rows_after_heading(burst, "* All M_AXI Variable Accesses", [
        "interface", "variable", "access_location", "direction", "burst_status",
        "length", "loop", "loop_location", "resolution", "problem",
    ])
    return HlsCompileReport(
        report_path=report_path,
        general=general,
        m_axi_interfaces=_rows_after_heading(hw, "* M_AXI", [
            "interface", "read_write", "data_width", "address_width", "latency", "offset",
            "register", "max_widen_bitwidth", "max_read_burst_length", "max_write_burst_length",
            "num_read_outstanding", "num_write_outstanding", "resource_estimate",
        ]),
        axilite_registers=_rows_after_heading(hw, "* S_AXILITE Registers", [
            "interface", "register", "offset", "width", "access", "description", "bit_fields",
        ]),
        top_arguments=_rows_after_heading(sw, "* Top Function Arguments", [
            "argument", "direction", "datatype",
        ]),
        sw_to_hw_mapping=_rows_after_heading(sw, "* SW-to-HW Mapping", [
            "argument", "hw_interface", "hw_type", "hw_usage", "hw_info",
        ]),
        burst_summary=_rows_after_heading(burst, "* Inferred Burst Summary", [
            "interface", "direction", "length", "width", "loop", "loop_location",
        ]),
        variable_accesses=variable_accesses,
        burst_status_counts=_count_by(variable_accesses, "burst_status"),
    )


def find_hls_compile_report(work_dir: Path) -> Path | None:
    candidates = [work_dir / "reports" / "hls_compile.rpt"]
    candidates.extend(sorted((work_dir / "reports").glob("*_compile.rpt")) if (work_dir / "reports").is_dir() else [])
    candidates.extend(sorted((work_dir / "hls" / "syn" / "report").glob("*_csynth.rpt")) if (work_dir / "hls" / "syn" / "report").is_dir() else [])
    for path in candidates:
        if path.is_file():
            return path
    return None
