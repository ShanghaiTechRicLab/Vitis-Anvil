#!/usr/bin/env python3
"""Parse a Vitis HLS csynth XML report and assert pipeline constraints.

Usage:
  parse_hls_report.py <csynth.xml> [--max-ii N] [--max-latency N] [--min-latency N]

Exit codes:
  0  all assertions pass
  1  some assertion failed; details on stderr
  2  argument / file error
"""
from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def worst_loop_ii(root: ET.Element) -> int:
    """Return max <PipelineII> across all loops in SummaryOfLoopLatency."""
    worst = 0
    for loop in root.iterfind(".//SummaryOfLoopLatency/*"):
        ii_elem = loop.find("PipelineII")
        if ii_elem is None or ii_elem.text is None:
            continue
        try:
            ii = int(ii_elem.text)
        except ValueError:
            continue
        if ii > worst:
            worst = ii
    return worst


def overall_latency(root: ET.Element) -> int | None:
    """Return Worst-caseLatency from SummaryOfOverallLatency, or None if undef."""
    elem = root.find(".//SummaryOfOverallLatency/Worst-caseLatency")
    if elem is None or elem.text is None or elem.text.strip() == "undef":
        return None
    try:
        return int(elem.text)
    except ValueError:
        return None


def sibling_aggregate_loop_ii(report: Path) -> int:
    """Return loop II from sibling aggregate csynth.xml when present."""
    aggregate = report.with_name("csynth.xml")
    if aggregate == report or not aggregate.exists():
        return 0

    try:
        tree = ET.parse(aggregate)
    except (ET.ParseError, OSError):
        return 0
    return worst_loop_ii(tree.getroot())


def main(argv: list[str]) -> int:
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
        tree = ET.parse(args.report)
    except OSError as e:
        print(f"error: report read failed: {e}", file=sys.stderr)
        return 2
    except ET.ParseError as e:
        print(f"error: XML parse failed: {e}", file=sys.stderr)
        return 2

    root = tree.getroot()
    ii = worst_loop_ii(root)
    if ii == 0:
        ii = max(ii, sibling_aggregate_loop_ii(args.report))
    lat = overall_latency(root)

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


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
