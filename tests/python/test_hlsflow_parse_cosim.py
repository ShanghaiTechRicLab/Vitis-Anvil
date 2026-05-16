"""Unit tests for hlsflow.parse_cosim (fast)."""
from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from hlsflow.parse_cosim import parse_cosim_dir  # noqa: E402


@pytest.mark.fast
def test_parse_cosim_vitis_2024_report(tmp_path: Path) -> None:
    report_dir = tmp_path / "sim" / "report"
    verilog_dir = report_dir / "verilog"
    verilog_dir.mkdir(parents=True)
    (report_dir / "saxpy_cosim.rpt").write_text(
        """Report time       : Sun May 17 03:38:56 AM CST 2026.
Solution          : hls.
Simulation tool   : xsim.

+----------+----------+-----------------------------------------------+-----------------------------------------------+----------------------+
|          |          |             Latency(Clock Cycles)             |              Interval(Clock Cycles)           | Total Execution Time |
+   RTL    +  Status  +-----------------------------------------------+-----------------------------------------------+    (Clock Cycles)    +
|          |          |      min      |      avg      |      max      |      min      |      avg      |      max      |                      |
+----------+----------+-----------------------------------------------+-----------------------------------------------+----------------------+
|      VHDL|        NA|             NA|             NA|             NA|             NA|             NA|             NA|                    NA|
|   Verilog|      Pass|            259|            259|            259|             NA|             NA|             NA|                   259|
+----------+----------+-----------------------------------------------+-----------------------------------------------+----------------------+
""",
        encoding="utf-8",
    )
    (verilog_dir / "lat.rpt").write_text(
        '$MAX_LATENCY = "259"\n$MIN_LATENCY = "259"\n$AVER_LATENCY = "259"\n$TOTAL_EXECUTE_TIME = "259"\n',
        encoding="utf-8",
    )
    (verilog_dir / "result.transaction.rpt").write_text(
        "                             latency        interval\ntransaction       0:             259               0\n",
        encoding="utf-8",
    )
    (verilog_dir / "saxpy.log").write_text(
        "INFO: [COSIM-1000] *** C/RTL co-simulation finished: PASS ***\n",
        encoding="utf-8",
    )

    rpt = parse_cosim_dir(report_dir)

    assert rpt.status == "pass"
    assert rpt.sim_tool == "xsim"
    assert rpt.solution == "hls"
    assert rpt.latency_cycles == 259
    assert rpt.passing_rtl is not None
    assert rpt.passing_rtl.rtl == "Verilog"
    assert rpt.passing_rtl.total_cycles == 259
    assert rpt.transaction_count == 1
    assert rpt.transactions[0].latency_cycles == 259
    assert rpt.lat_summary["max_latency"] == 259
    assert any("COSIM-1000" in msg for msg in rpt.messages)
