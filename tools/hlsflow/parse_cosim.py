"""Parse Vitis HLS cosim reports — multi-format fallback."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re
import xml.etree.ElementTree as ET

_RPT_GLOBS = (
    "*_cosim.rpt",
    "cosim.rpt",
    "lat.rpt",
    "verilog/lat.rpt",
    "verilog/result.transaction.rpt",
    "verilog/*_cosim.rpt",
    "verilog/*.log",
)
_XML_GLOBS = ("cosim.xml", "*_cosim.xml", "cosim_options.xml")
_PASS_RE = re.compile(r"\b(PASS|PASSED|Pass)\b")
_FAIL_RE = re.compile(r"\b(FAIL|FAILED|Fail)\b")
_LAT_RE = re.compile(r"(?:Latency|cycle).*?(\d+)", re.IGNORECASE)
_KV_RE = re.compile(r'^\$(?P<key>[A-Z_]+)\s*=\s*"(?P<value>-?\d+)"')
_TRANSACTION_RE = re.compile(r"transaction\s+(?P<idx>\d+)\s*:\s*(?P<lat>\d+)\s+(?P<interval>-?\d+)", re.IGNORECASE)
_RTL_ROW_RE = re.compile(
    r"^\|\s*(?P<rtl>VHDL|Verilog)\s*\|\s*(?P<status>[^|]+?)\s*\|"
    r"\s*(?P<lat_min>NA|\d+)\s*\|\s*(?P<lat_avg>NA|\d+)\s*\|\s*(?P<lat_max>NA|\d+)\s*\|"
    r"\s*(?P<int_min>NA|\d+)\s*\|\s*(?P<int_avg>NA|\d+)\s*\|\s*(?P<int_max>NA|\d+)\s*\|"
    r"\s*(?P<total>NA|\d+)\s*\|"
)


def _int_or_none(value: str | None) -> int | None:
    if value is None:
        return None
    text = value.strip()
    return int(text) if text.isdigit() else None


@dataclass
class RtlCosimResult:
    rtl: str
    status: str
    latency_min: int | None = None
    latency_avg: int | None = None
    latency_max: int | None = None
    interval_min: int | None = None
    interval_avg: int | None = None
    interval_max: int | None = None
    total_cycles: int | None = None


@dataclass
class CosimTransaction:
    index: int
    latency_cycles: int
    interval_cycles: int | None


@dataclass
class CosimReport:
    report_dir: Path
    found_files: list[Path]
    status: str
    latency_cycles: int | None
    raw_text: str | None
    report_time: str | None = None
    solution: str | None = None
    sim_tool: str | None = None
    rtl_results: list[RtlCosimResult] = field(default_factory=list)
    transactions: list[CosimTransaction] = field(default_factory=list)
    lat_summary: dict[str, int] = field(default_factory=dict)
    messages: list[str] = field(default_factory=list)

    @property
    def passing_rtl(self) -> RtlCosimResult | None:
        for row in self.rtl_results:
            if row.status.lower() == "pass":
                return row
        return self.rtl_results[0] if self.rtl_results else None

    @property
    def transaction_count(self) -> int:
        return len(self.transactions)


def _parse_text_report(text: str, report: CosimReport) -> None:
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("Report time"):
            report.report_time = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("Solution"):
            report.solution = stripped.split(":", 1)[1].strip().rstrip(".")
        elif stripped.startswith("Simulation tool"):
            report.sim_tool = stripped.split(":", 1)[1].strip().rstrip(".")

        row = _RTL_ROW_RE.match(stripped)
        if row:
            report.rtl_results.append(RtlCosimResult(
                rtl=row.group("rtl"),
                status=row.group("status").strip(),
                latency_min=_int_or_none(row.group("lat_min")),
                latency_avg=_int_or_none(row.group("lat_avg")),
                latency_max=_int_or_none(row.group("lat_max")),
                interval_min=_int_or_none(row.group("int_min")),
                interval_avg=_int_or_none(row.group("int_avg")),
                interval_max=_int_or_none(row.group("int_max")),
                total_cycles=_int_or_none(row.group("total")),
            ))
            continue

        kv = _KV_RE.match(stripped)
        if kv:
            report.lat_summary[kv.group("key").lower()] = int(kv.group("value"))
            continue

        txn = _TRANSACTION_RE.search(stripped)
        if txn:
            interval = int(txn.group("interval"))
            report.transactions.append(CosimTransaction(
                index=int(txn.group("idx")),
                latency_cycles=int(txn.group("lat")),
                interval_cycles=interval if interval >= 0 else None,
            ))
            continue

        if "COSIM-" in stripped or "C/RTL co-simulation" in stripped:
            report.messages.append(stripped)


def parse_cosim_dir(sim_report_dir: Path) -> CosimReport:
    if not sim_report_dir.is_dir():
        return CosimReport(sim_report_dir, [], "unknown", None, None)
    found: list[Path] = []
    for glob in _RPT_GLOBS + _XML_GLOBS:
        found.extend(sim_report_dir.glob(glob))
    found = sorted(set(found))
    report = CosimReport(sim_report_dir, found, "unknown", None, None)
    snippet: str | None = None
    for rpt in found:
        if rpt.suffix.lower() == ".xml":
            try:
                root = ET.parse(rpt).getroot()
            except ET.ParseError:
                continue
            for tag in (".//Status", ".//CosimStatus", ".//Result"):
                node = root.find(tag)
                if node is not None and node.text:
                    txt = node.text.strip().upper()
                    if "PASS" in txt:
                        report.status = "pass"
                    elif "FAIL" in txt:
                        report.status = "fail"
            for tag in (".//Latency", ".//OverallLatency"):
                node = root.find(tag)
                if node is not None and node.text and node.text.strip().isdigit():
                    report.latency_cycles = int(node.text.strip())
        else:
            try:
                text = rpt.read_text(errors="replace")
            except OSError:
                continue
            if snippet is None:
                snippet = text[:2000]
            _parse_text_report(text, report)
            if _FAIL_RE.search(text):
                report.status = "fail"
            elif _PASS_RE.search(text) and report.status != "fail":
                report.status = "pass"
            if report.latency_cycles is None:
                m = _LAT_RE.search(text)
                if m:
                    report.latency_cycles = int(m.group(1))
    if report.latency_cycles is None:
        row = report.passing_rtl
        if row and row.latency_max is not None:
            report.latency_cycles = row.latency_max
    if report.latency_cycles is None and "max_latency" in report.lat_summary:
        report.latency_cycles = report.lat_summary["max_latency"]
    report.raw_text = snippet
    return report
