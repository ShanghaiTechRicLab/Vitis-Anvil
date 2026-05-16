"""Parse Vitis HLS cosim reports — multi-format fallback."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import xml.etree.ElementTree as ET

_RPT_GLOBS = ("*_cosim.rpt", "cosim.rpt", "lat.rpt", "verilog/*_cosim.rpt")
_XML_GLOBS = ("cosim.xml", "*_cosim.xml")
_PASS_RE = re.compile(r"\b(PASS|PASSED|Pass)\b")
_FAIL_RE = re.compile(r"\b(FAIL|FAILED|Fail)\b")
_LAT_RE = re.compile(r"(?:Latency|cycle).*?(\d+)", re.IGNORECASE)


@dataclass
class CosimReport:
    report_dir: Path
    found_files: list[Path]
    status: str
    latency_cycles: int | None
    raw_text: str | None


def parse_cosim_dir(sim_report_dir: Path) -> CosimReport:
    if not sim_report_dir.is_dir():
        return CosimReport(sim_report_dir, [], "unknown", None, None)
    found: list[Path] = []
    for glob in _RPT_GLOBS + _XML_GLOBS:
        found.extend(sim_report_dir.glob(glob))
    found = sorted(set(found))
    status = "unknown"
    latency: int | None = None
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
                        status = "pass"
                    elif "FAIL" in txt:
                        status = "fail"
            for tag in (".//Latency", ".//OverallLatency"):
                node = root.find(tag)
                if node is not None and node.text and node.text.strip().isdigit():
                    latency = int(node.text.strip())
        else:
            try:
                text = rpt.read_text(errors="replace")
            except OSError:
                continue
            if snippet is None:
                snippet = text[:2000]
            if _FAIL_RE.search(text):
                status = "fail"
            elif _PASS_RE.search(text) and status != "fail":
                status = "pass"
            if latency is None:
                m = _LAT_RE.search(text)
                if m:
                    latency = int(m.group(1))
    return CosimReport(sim_report_dir, found, status, latency, snippet)
