from pathlib import Path

from hlsflow.parse_hls_compile import parse_hls_compile_report


def test_parse_hls_compile_report_extracts_io_and_bursts():
    rpt = parse_hls_compile_report(Path("tests/python/fixtures/hls_compile_report.rpt"))
    assert rpt.general["version"].startswith("2024.2")
    assert rpt.general["target_device"] == "xcu250-figd2104-2L-e"
    assert rpt.m_axi_interfaces[0]["interface"] == "m_axi_gmem0"
    assert rpt.axilite_registers[1]["offset"] == "0x34"
    assert rpt.top_arguments[0]["datatype"] == "DataPack<float, 16>*"
    assert rpt.sw_to_hw_mapping[1]["hw_interface"] == "s_axi_control"
    assert rpt.burst_summary[0]["width"] == "512"
    assert rpt.burst_status_counts == {"Widen Fail": 1, "Inferred": 1}
    assert rpt.variable_accesses[0]["resolution"] == "214-353"
