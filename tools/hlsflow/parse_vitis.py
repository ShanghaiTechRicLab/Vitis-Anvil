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


def find_and_parse_logs(work_dir: Path, kernel: str) -> list[VitisLogReport]:
    """Discover and parse logs for one kernel HLS work directory.

    The collect/csynth path must not scan the whole CMake build tree: a stale
    *_xclbin/link/v++.log from a failed link run is unrelated to HLS synthesis
    and would otherwise be reported as a false csynth error.
    """
    results: list[VitisLogReport] = []

    # Current Vitis --mode hls logs are normally under <kernel>_hls/logs/.
    for name in ("hls_compile.log", "vitis_hls.log"):
        hit = work_dir / "logs" / name
        if hit.is_file():
            results.append(parse_hls_log(hit))
            break

    # Older / alternate layouts can put vitis_hls.log deeper in the HLS project.
    if not results:
        matched = [p for p in sorted(work_dir.rglob("vitis_hls.log")) if kernel in str(p)]
        fallback = sorted(work_dir.rglob("vitis_hls.log"))
        for hit in (matched or fallback)[:1]:
            results.append(parse_hls_log(hit))

    # Parse v++ logs only inside the kernel HLS work dir; explicitly ignore
    # xclbin/link logs, which belong to the separate link stage.
    for vpp_log in sorted(work_dir.rglob("v++*.log"))[:5]:
        if "_xclbin" in vpp_log.parts or "run_link" in vpp_log.parts:
            continue
        results.append(parse_vpp_log(vpp_log))

    return results
