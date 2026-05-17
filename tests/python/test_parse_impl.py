from pathlib import Path

from hlsflow.parse_impl import find_impl_report, parse_impl_report


def test_parse_hls_impl_report_fixture():
    rpt = parse_impl_report(Path("tests/python/fixtures/hls_impl_report.rpt"))
    assert rpt.implementation_tool == "Xilinx Vivado v.2022.2.2"
    assert rpt.project == "saxpy_hls"
    assert rpt.solution == "hls"
    assert rpt.device == "xcu50-fsvh2104-2-e"
    assert rpt.resources["LUT"] == 8218
    assert rpt.resources["FF"] == 13599
    assert rpt.resources["DSP"] == 80
    assert rpt.resources["BRAM"] == 46
    assert rpt.resources["CLB"] == 1911
    assert rpt.cp_required_ns == 3.333
    assert rpt.cp_achieved_post_synthesis_ns == 2.305
    assert rpt.cp_achieved_post_implementation_ns == 3.038
    assert rpt.post_impl_slack_ns == 0.2950000000000004
    assert rpt.timing_met is True


def test_find_impl_report_discovers_post_impl_report(tmp_path):
    report_dir = tmp_path / "hls" / "impl" / "report"
    report_dir.mkdir(parents=True)
    impl = report_dir / "saxpy_export.rpt"
    impl.write_text(Path("tests/python/fixtures/hls_impl_report.rpt").read_text(), encoding="utf-8")
    (report_dir / "noise.rpt").write_text("not an implementation report", encoding="utf-8")
    assert find_impl_report(tmp_path) == impl
