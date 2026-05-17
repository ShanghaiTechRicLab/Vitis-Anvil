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
    tool_versions: list[dict[str, str]] = field(default_factory=list)
    phase_times: list[dict[str, float | str]] = field(default_factory=list)
    total_cpu_user_sec: float | None = None
    total_cpu_system_sec: float | None = None
    total_elapsed_sec: float | None = None
    peak_memory_mb: float | None = None


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
_TOOL_VERSION = re.compile(r"^\*+\s*(?P<tool>v\+\+|vitis-run|Vitis HLS|Vivado)[^\n]*?v(?P<version>[0-9]{4}\.[0-9](?:\.[0-9])?)", re.MULTILINE)
_PHASE_TIME = re.compile(
    r"Finished (?P<phase>.*?): CPU user time: (?P<user>[0-9.]+) seconds\. "
    r"CPU system time: (?P<system>[0-9.]+) seconds\. "
    r"Elapsed time: (?P<elapsed>[0-9.]+) seconds; current allocated memory: (?P<memory>[0-9.]+) MB\.",
    re.IGNORECASE,
)
_TOTAL_TIME = re.compile(
    r"Total CPU user time: (?P<user>[0-9.]+) seconds\. "
    r"Total CPU system time: (?P<system>[0-9.]+) seconds\. "
    r"Total elapsed time: (?P<elapsed>[0-9.]+) seconds; peak allocated memory: (?P<memory>[0-9.]+) MB\.",
    re.IGNORECASE,
)


def _grep(text: str, pat: re.Pattern[str], cap: int = 20) -> list[str]:
    return [m.group(0).strip() for m in pat.finditer(text)][:cap]




def _parse_log_metrics(text: str, rpt: VitisLogReport) -> None:
    rpt.tool_versions = [m.groupdict() for m in _TOOL_VERSION.finditer(text)]
    for m in _PHASE_TIME.finditer(text):
        rpt.phase_times.append({
            "phase": m.group("phase").strip(),
            "cpu_user_sec": float(m.group("user")),
            "cpu_system_sec": float(m.group("system")),
            "elapsed_sec": float(m.group("elapsed")),
            "memory_mb": float(m.group("memory")),
        })
    totals = list(_TOTAL_TIME.finditer(text))
    if totals:
        last = totals[-1]
        rpt.total_cpu_user_sec = float(last.group("user"))
        rpt.total_cpu_system_sec = float(last.group("system"))
        rpt.total_elapsed_sec = float(last.group("elapsed"))
        rpt.peak_memory_mb = float(last.group("memory"))

def parse_vpp_log(log_path: Path) -> VitisLogReport:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    rpt = VitisLogReport(log_path=str(log_path))
    rpt.errors = _grep(text, _VPP_ERROR)
    rpt.warnings = _grep(text, _VPP_WARN)
    rpt.timing_violations = _grep(text, _VPP_TIMING, cap=10)
    if rpt.timing_violations:
        rpt.timing_met = False
    _parse_log_metrics(text, rpt)
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
    _parse_log_metrics(text, rpt)
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
