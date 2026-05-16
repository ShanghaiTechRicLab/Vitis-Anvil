"""Tests for hlsflow.discover (fast, tmp tree)."""
from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from hlsflow.discover import find_csynth_for_kernel, find_csynth_reports  # noqa: E402


def _make_kernel_tree(root: Path, kernel: str, has_sub: bool = True) -> Path:
    work = root / "src" / "kernels" / f"{kernel}_hls"
    rpt = work / "hls" / "syn" / "report"
    rpt.mkdir(parents=True)
    (rpt / f"{kernel}_csynth.xml").write_text("<profile/>")
    if has_sub:
        (rpt / f"{kernel}_Pipeline_VITIS_LOOP_1.xml").write_text("<profile/>")
        (rpt / f"{kernel}_Loop_inner.xml").write_text("<profile/>")
    return work


@pytest.mark.fast
def test_discover_single_kernel(tmp_path: Path) -> None:
    _make_kernel_tree(tmp_path, "saxpy")
    hits = find_csynth_reports(tmp_path)
    assert len(hits) == 1
    assert hits[0].kernel == "saxpy"
    assert hits[0].report_path.name == "saxpy_csynth.xml"


@pytest.mark.fast
def test_discover_skips_sub_reports(tmp_path: Path) -> None:
    _make_kernel_tree(tmp_path, "saxpy", has_sub=True)
    assert len(find_csynth_reports(tmp_path)) == 1


@pytest.mark.fast
def test_discover_multi_kernel(tmp_path: Path) -> None:
    _make_kernel_tree(tmp_path, "saxpy")
    _make_kernel_tree(tmp_path, "vadd")
    hits = sorted(find_csynth_reports(tmp_path), key=lambda h: h.kernel)
    assert [h.kernel for h in hits] == ["saxpy", "vadd"]


@pytest.mark.fast
def test_find_by_kernel(tmp_path: Path) -> None:
    _make_kernel_tree(tmp_path, "saxpy")
    _make_kernel_tree(tmp_path, "vadd")
    hit = find_csynth_for_kernel(tmp_path, "saxpy")
    assert hit is not None and hit.kernel == "saxpy"
    assert find_csynth_for_kernel(tmp_path, "missing") is None


@pytest.mark.fast
def test_discover_under_u55c_build(tmp_path: Path) -> None:
    _make_kernel_tree(tmp_path, "saxpy")
    hits = find_csynth_reports(tmp_path)
    assert len(hits) == 1
    assert hits[0].kernel == "saxpy"
