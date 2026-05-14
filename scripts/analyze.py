#!/usr/bin/env python3
"""Parse HLS csynth XML report and print resource/timing summary."""
from __future__ import annotations

from pathlib import Path
import sys
import xml.etree.ElementTree as ET

import click

import anvil.log as log
import anvil.table as tbl


def find_report(build_dir: Path) -> Path | None:
    reports = sorted(build_dir.rglob("*_csynth.xml"))
    if not reports:
        return None
    top_reports = [path for path in reports if "Pipeline" not in path.name and "Loop" not in path.name]
    for path in top_reports:
        if path.name == "saxpy_csynth.xml":
            return path
    return min(top_reports or reports, key=lambda path: (len(path.name), path.name))


@click.command()
@click.option("--build-dir", default="build/hls-model-linux-debug", help="CMake build directory")
@click.option("--report", default=None, help="Direct path to *_csynth.xml")
def main(build_dir: str, report: str | None) -> None:
    log.init("analyze")
    rpt = Path(report) if report else find_report(Path(build_dir))
    if rpt is None or not rpt.exists():
        log.error("No csynth XML report found — run 'make csynth' first.")
        sys.exit(1)

    log.info("parsing {}", rpt)
    print(f"report: {rpt}")
    try:
        root = ET.parse(rpt).getroot()
    except ET.ParseError as exc:
        log.error("failed to parse {}: {}", rpt, exc)
        sys.exit(1)

    def get(*tags: str) -> str:
        for tag in tags:
            node = root.find(f".//{tag}")
            if node is not None and node.text and node.text.strip():
                return node.text.strip()
        return "—"

    rows = [
        ["Metric", "Value"],
        ["Latency (cycles)", get("Latency", "Best-caseLatency", "Average-caseLatency", "Worst-caseLatency")],
        ["II (initiation interval)", get("II", "Interval-min", "Interval-max")],
        ["PipelineII", get("PipelineII")],
        ["LUT", get("LUT")],
        ["FF", get("FF")],
        ["DSP", get("DSP")],
        ["BRAM_18K", get("BRAM_18K")],
    ]
    tbl.print_table(rows)


if __name__ == "__main__":
    main()
