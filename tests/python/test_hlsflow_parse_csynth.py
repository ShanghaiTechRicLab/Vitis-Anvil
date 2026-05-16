"""Unit tests for hlsflow.parse_csynth (fast)."""
from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from hlsflow.parse_csynth import parse_csynth_report  # noqa: E402

FIXTURE = Path(__file__).parent / "fixtures" / "csynth_saxpy.xml"


@pytest.mark.fast
def test_parse_csynth_basic() -> None:
    rpt = parse_csynth_report(FIXTURE)
    assert rpt.worst_loop_ii >= 1
    assert rpt.latency_max is not None
    assert "LUT" in rpt.resources
    assert "LUT" in rpt.available
    assert rpt.utilization["LUT"] >= 0.0


@pytest.mark.fast
def test_parse_csynth_clock() -> None:
    rpt = parse_csynth_report(FIXTURE)
    assert rpt.target_clock_ns is not None
    assert rpt.estimated_clock_ns is not None
    assert rpt.timing_met is True


@pytest.mark.fast
def test_parse_csynth_loops_nonempty() -> None:
    rpt = parse_csynth_report(FIXTURE)
    assert len(rpt.loops) >= 1
