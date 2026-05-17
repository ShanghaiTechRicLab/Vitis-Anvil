from pathlib import Path

from hlsflow.parse_vitis import parse_hls_log


def test_parse_hls_log_phase_times_and_totals():
    rpt = parse_hls_log(Path("tests/python/fixtures/vitis_hls_timing.log"))
    assert rpt.tool_versions == [{"tool": "Vitis HLS", "version": "2024.2"}]
    assert len(rpt.phase_times) == 2
    assert rpt.phase_times[0]["phase"] == "File checks and directory preparation"
    assert rpt.phase_times[0]["elapsed_sec"] == 2.15
    assert rpt.total_cpu_user_sec == 2.09
    assert rpt.total_cpu_system_sec == 0.18
    assert rpt.total_elapsed_sec == 2.16
    assert rpt.peak_memory_mb == 659.062
