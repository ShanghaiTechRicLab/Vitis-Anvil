"""Parse v++.log and vitis_hls.log for errors, warnings, and timing issues."""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class VitisLogReport:
    log_path: str = ""
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    timing_violations: list[str] = field(default_factory=list)
    timing_met: bool | None = None  # None = not determinable from this log


# v++.log patterns (Vitis 2022.x–2024.x)
_VPP_ERROR = re.compile(r"(?:\[ERROR\]|^ERROR:\s+\[V\+\+)[^\n]*", re.IGNORECASE | re.MULTILINE)
_VPP_WARN = re.compile(r"(?:\[WARNING\]|^WARNING:\s+\[V\+\+)[^\n]*", re.IGNORECASE | re.MULTILINE)
_VPP_TIMING = re.compile(
    r"timing\s+violation|failed\s+timing|not\s+met|slack.*negative", re.IGNORECASE)

# vitis_hls.log patterns — covers both @E/@W (2022.x) and ERROR:/WARNING: (2021.x)
_HLS_ERROR = re.compile(r"(^@E\s+\[|^ERROR:\s+\[HLS)", re.MULTILINE)
_HLS_WARN = re.compile(r"(^@W\s+\[|^WARNING:\s+\[HLS)", re.MULTILINE)
_HLS_TIMING = re.compile(
    r"Unable to schedule|Timing not met|critical path", re.IGNORECASE)


def _grep(text: str, pat: re.Pattern[str], cap: int = 20) -> list[str]:
    return [m.group(0).strip() for m in pat.finditer(text)][:cap]


def parse_vpp_log(log_path: Path) -> VitisLogReport:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    rpt = VitisLogReport(log_path=str(log_path))
    rpt.errors = _grep(text, _VPP_ERROR)
    rpt.warnings = _grep(text, _VPP_WARN)
    rpt.timing_violations = _grep(text, _VPP_TIMING, cap=10)
    if rpt.timing_violations:
        rpt.timing_met = False
    return rpt


def parse_hls_log(log_path: Path) -> VitisLogReport:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    rpt = VitisLogReport(log_path=str(log_path))
    rpt.errors = [line.strip() for line in lines if _HLS_ERROR.match(line)][:20]
    rpt.warnings = [line.strip() for line in lines if _HLS_WARN.match(line)][:20]
    rpt.timing_violations = _grep(text, _HLS_TIMING, cap=10)
    if rpt.timing_violations:
        rpt.timing_met = False
    return rpt


def find_and_parse_logs(build_dir: Path, kernel: str) -> list[VitisLogReport]:
    """Discover and parse all relevant Vitis logs under build_dir for kernel."""
    results: list[VitisLogReport] = []

    # vitis_hls.log is inside the HLS project dir — prefer the one that contains
    # the kernel name in its path.
    matched = [p for p in sorted(build_dir.rglob("vitis_hls.log")) if kernel in str(p)]
    fallback = sorted(build_dir.rglob("vitis_hls.log"))
    for hit in (matched or fallback)[:1]:
        results.append(parse_hls_log(hit))

    # v++ compile/link logs.
    for vpp_log in sorted(build_dir.rglob("v++*.log"))[:5]:
        results.append(parse_vpp_log(vpp_log))

    return results
