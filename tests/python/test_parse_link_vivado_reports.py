from pathlib import Path

from hlsflow.parse_link import (
    parse_vivado_resource_summary_report,
    parse_vivado_timing_report,
    parse_vivado_utilization_report,
)


def test_parse_vivado_utilization_report():
    util = parse_vivado_utilization_report(Path("tests/python/fixtures/kernel_util_routed.rpt"))
    assert util["Used Resources"]["LUT"]["used"] == 16789
    assert util["Used Resources"]["DSP"]["pct"] == 0.91
    assert util["saxpy"]["BRAM"]["used"] == 23


def test_parse_vivado_timing_report():
    timing = parse_vivado_timing_report(Path("tests/python/fixtures/dr_timing_summary.rpt"))
    assert timing["WNS"] == 0.057
    assert timing["TNS_failing_endpoints"] == 0.0
    assert timing["WHS"] == 0.010
    assert timing["timing_met"] is True


def test_parse_vivado_resource_summary_report():
    summary = parse_vivado_resource_summary_report(Path("tests/python/fixtures/vivado_full_util.rpt"))
    assert summary["metadata"]["device"] == "xcu250-figd2104-2L-e"
    rows = summary["rows"]
    assert rows["CLB LUTs"]["used"] == 154393
    assert rows["CLB Registers"]["utilpct"] == 7.27
    assert rows["Block RAM Tile"]["used"] == 311.5
    assert rows["URAM"]["available"] == 1280
