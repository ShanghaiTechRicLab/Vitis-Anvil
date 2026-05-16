"""Unit tests for Vitis log discovery."""
from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from hlsflow.parse_vitis import find_and_parse_logs  # noqa: E402


@pytest.mark.fast
def test_find_and_parse_logs_ignores_sibling_xclbin_link_errors(tmp_path: Path) -> None:
    work_dir = tmp_path / "build" / "src" / "kernels" / "saxpy_hls"
    logs_dir = work_dir / "logs"
    logs_dir.mkdir(parents=True)
    (logs_dir / "hls_compile.log").write_text(
        "INFO: [HLS 200-10] csynth ok\n",
        encoding="utf-8",
    )

    link_dir = tmp_path / "build" / "src" / "kernels" / "saxpy_xclbin" / "work" / "link" / "run_link"
    link_dir.mkdir(parents=True)
    (link_dir / "v++.log").write_text(
        "ERROR: [v++ 60-661] v++ link run 'run_link' failed\n",
        encoding="utf-8",
    )

    reports = find_and_parse_logs(work_dir, "saxpy")

    assert reports
    assert [err for rpt in reports for err in rpt.errors] == []
