"""Parse Vitis HLS post-implementation text reports.

These reports are emitted by Vitis/Vivado after HLS export/implementation and
contain final resource/timing numbers that are not present in csynth XML.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass
class HlsImplementationReport:
    report_path: Path
    implementation_tool: str | None = None
    project: str | None = None
    solution: str | None = None
    device: str | None = None
    report_date: str | None = None
    resources: dict[str, int] = field(default_factory=dict)
    cp_required_ns: float | None = None
    cp_achieved_post_synthesis_ns: float | None = None
    cp_achieved_post_implementation_ns: float | None = None
    timing_met: bool | None = None

    @property
    def post_impl_slack_ns(self) -> float | None:
        if self.cp_required_ns is None or self.cp_achieved_post_implementation_ns is None:
            return None
        return self.cp_required_ns - self.cp_achieved_post_implementation_ns


_KEY_VALUE_PATTERNS = {
    "implementation_tool": re.compile(r"^Implementation tool:\s*(.+?)\s*$", re.MULTILINE),
    "project": re.compile(r"^Project:\s*(.+?)\s*$", re.MULTILINE),
    "solution": re.compile(r"^Solution:\s*(.+?)\s*$", re.MULTILINE),
    "device": re.compile(r"^Device target:\s*(.+?)\s*$", re.MULTILINE),
    "report_date": re.compile(r"^Report date:\s*(.+?)\s*$", re.MULTILINE),
}
_RESOURCE_RE = re.compile(r"^(SLICE|LUT|FF|DSP|BRAM|URAM|LATCH|SRL|CLB):\s*([0-9,]+)\s*$", re.MULTILINE)
_FLOAT_RE = r"([+-]?(?:\d+(?:\.\d*)?|\.\d+))"
_CP_REQUIRED_RE = re.compile(rf"^CP required:\s*{_FLOAT_RE}\s*$", re.MULTILINE)
_CP_POST_SYN_RE = re.compile(rf"^CP achieved post-synthesis:\s*{_FLOAT_RE}\s*$", re.MULTILINE)
_CP_POST_IMPL_RE = re.compile(rf"^CP achieved post-implementation:\s*{_FLOAT_RE}\s*$", re.MULTILINE)


def _match_text(pattern: re.Pattern[str], text: str) -> str | None:
    m = pattern.search(text)
    return m.group(1).strip() if m else None


def _match_float(pattern: re.Pattern[str], text: str) -> float | None:
    m = pattern.search(text)
    if not m:
        return None
    try:
        return float(m.group(1))
    except ValueError:
        return None


def parse_impl_report(report_path: Path) -> HlsImplementationReport:
    text = report_path.read_text(encoding="utf-8", errors="replace")
    values = {name: _match_text(pattern, text) for name, pattern in _KEY_VALUE_PATTERNS.items()}
    resources = {m.group(1): int(m.group(2).replace(",", "")) for m in _RESOURCE_RE.finditer(text)}
    timing_met: bool | None
    if re.search(r"^Timing met\s*$", text, flags=re.MULTILINE | re.IGNORECASE):
        timing_met = True
    elif re.search(r"^Timing (?:not met|failed)\s*$", text, flags=re.MULTILINE | re.IGNORECASE):
        timing_met = False
    else:
        timing_met = None
    return HlsImplementationReport(
        report_path=report_path,
        implementation_tool=values["implementation_tool"],
        project=values["project"],
        solution=values["solution"],
        device=values["device"],
        report_date=values["report_date"],
        resources=resources,
        cp_required_ns=_match_float(_CP_REQUIRED_RE, text),
        cp_achieved_post_synthesis_ns=_match_float(_CP_POST_SYN_RE, text),
        cp_achieved_post_implementation_ns=_match_float(_CP_POST_IMPL_RE, text),
        timing_met=timing_met,
    )


def find_impl_reports(work_dir: Path) -> list[Path]:
    """Find post-implementation reports under a kernel HLS work directory."""
    hits: list[Path] = []
    for rpt in sorted(work_dir.rglob("*.rpt")):
        try:
            head = rpt.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "Post-Implementation Resource usage" in head and "CP achieved post-implementation" in head:
            hits.append(rpt)
    return hits


def find_impl_report(work_dir: Path) -> Path | None:
    hits = find_impl_reports(work_dir)
    if not hits:
        return None
    # Prefer reports directly under hls/impl before misc copies.
    return sorted(hits, key=lambda p: ("/misc/" in p.as_posix(), len(p.parts), p.as_posix()))[0]
