"""Parse a Vitis HLS csynth XML report."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import xml.etree.ElementTree as ET


@dataclass
class LoopInfo:
    name: str
    pipeline_ii: int | None
    trip_count: int | None
    latency: int | None
    pipelined: bool


@dataclass
class CsynthReport:
    report_path: Path
    top: str
    target_clock_ns: float | None
    estimated_clock_ns: float | None
    timing_met: bool | None
    latency_min: int | None
    latency_max: int | None
    worst_loop_ii: int
    resources: dict[str, int] = field(default_factory=dict)
    available: dict[str, int] = field(default_factory=dict)
    loops: list[LoopInfo] = field(default_factory=list)

    @property
    def utilization(self) -> dict[str, float]:
        out: dict[str, float] = {}
        for k, used in self.resources.items():
            avail = self.available.get(k)
            if avail and avail > 0:
                out[k] = used / avail
        return out


_RESOURCE_KEYS = ("LUT", "FF", "DSP", "BRAM_18K", "URAM")


def _find_text(root: ET.Element, paths: list[str]) -> str | None:
    for p in paths:
        node = root.find(p)
        if node is not None and node.text and node.text.strip() and node.text.strip() != "undef":
            return node.text.strip()
    return None


def _find_int(root: ET.Element, paths: list[str]) -> int | None:
    txt = _find_text(root, paths)
    if txt is None:
        return None
    try:
        return int(txt)
    except ValueError:
        return None


def _find_float(root: ET.Element, paths: list[str]) -> float | None:
    txt = _find_text(root, paths)
    if txt is None:
        return None
    try:
        return float(txt)
    except ValueError:
        return None


def _worst_loop_ii(root: ET.Element) -> int:
    worst = 0
    for loop in root.iterfind(".//SummaryOfLoopLatency/*"):
        ii = loop.find("PipelineII")
        if ii is None or ii.text is None:
            continue
        try:
            v = int(ii.text)
        except ValueError:
            continue
        worst = max(worst, v)
    return worst


def _sibling_aggregate_ii(report: Path) -> int:
    aggregate = report.with_name("csynth.xml")
    if aggregate == report or not aggregate.exists():
        return 0
    try:
        tree = ET.parse(aggregate)
    except (ET.ParseError, OSError):
        return 0
    return _worst_loop_ii(tree.getroot())


def _collect_loops(root: ET.Element) -> list[LoopInfo]:
    loops: list[LoopInfo] = []
    for elem in root.iterfind(".//SummaryOfLoopLatency/*"):
        name_node = elem.find("Name")
        name = name_node.text.strip() if (name_node is not None and name_node.text) else elem.tag
        ii = _find_int(elem, ["PipelineII"])
        trip = _find_int(elem, ["TripCount"])
        lat = _find_int(elem, ["Latency", "Worst-caseLatency"])
        loops.append(LoopInfo(name=name, pipeline_ii=ii, trip_count=trip,
                              latency=lat, pipelined=ii is not None and ii > 0))
    return loops


def parse_csynth_report(report_path: Path) -> CsynthReport:
    tree = ET.parse(report_path)
    root = tree.getroot()

    top_node = root.find(".//UserAssignments/TopModelName")
    top = top_node.text.strip() if (top_node is not None and top_node.text) else report_path.stem.replace("_csynth", "")

    target_clk = _find_float(root, [".//SummaryOfTimingAnalysis/TargetClockPeriod", ".//UserAssignments/TargetClockPeriod"])
    est_clk = _find_float(root, [".//SummaryOfTimingAnalysis/EstimatedClockPeriod"])
    timing_met = None if (target_clk is None or est_clk is None) else est_clk <= target_clk

    lat_min = _find_int(root, [".//SummaryOfOverallLatency/Best-caseLatency"])
    lat_max = _find_int(root, [".//SummaryOfOverallLatency/Worst-caseLatency"])

    ii = _worst_loop_ii(root)
    if ii == 0:
        ii = max(ii, _sibling_aggregate_ii(report_path))

    resources: dict[str, int] = {}
    available: dict[str, int] = {}
    for key in _RESOURCE_KEYS:
        v = _find_int(root, [f".//AreaEstimates/Resources/{key}"])
        if v is not None:
            resources[key] = v
        a = _find_int(root, [f".//AreaEstimates/AvailableResources/{key}"])
        if a is not None:
            available[key] = a

    return CsynthReport(
        report_path=report_path,
        top=top,
        target_clock_ns=target_clk,
        estimated_clock_ns=est_clk,
        timing_met=timing_met,
        latency_min=lat_min,
        latency_max=lat_max,
        worst_loop_ii=ii,
        resources=resources,
        available=available,
        loops=_collect_loops(root),
    )


def legacy_cli(argv: list[str]) -> int:
    """Compatibility CLI for cmake/parse_hls_report.py."""
    import argparse
    import sys

    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path, help="Path to <top>_csynth.xml")
    parser.add_argument("--max-ii", type=int, default=None,
                        help="Fail if worst-case loop II exceeds this value")
    parser.add_argument("--max-latency", type=int, default=None,
                        help="Fail if overall worst-case latency exceeds this value")
    parser.add_argument("--min-latency", type=int, default=None,
                        help="Fail if overall worst-case latency is below this value")
    args = parser.parse_args(argv)

    if not args.report.exists():
        print(f"error: report not found: {args.report}", file=sys.stderr)
        return 2
    try:
        rpt = parse_csynth_report(args.report)
    except OSError as e:
        print(f"error: report read failed: {e}", file=sys.stderr)
        return 2
    except ET.ParseError as e:
        print(f"error: XML parse failed: {e}", file=sys.stderr)
        return 2

    ii = rpt.worst_loop_ii
    lat = rpt.latency_max

    print(f"worst_loop_ii  = {ii}", flush=True)
    print(f"overall_latency = {lat}", flush=True)

    failed = False
    if args.max_ii is not None and ii > args.max_ii:
        print(f"FAIL: worst loop II {ii} > max {args.max_ii}", file=sys.stderr)
        failed = True
    if args.max_latency is not None and lat is not None and lat > args.max_latency:
        print(f"FAIL: latency {lat} > max {args.max_latency}", file=sys.stderr)
        failed = True
    if args.min_latency is not None and lat is not None and lat < args.min_latency:
        print(f"FAIL: latency {lat} < min {args.min_latency}", file=sys.stderr)
        failed = True
    return 1 if failed else 0
