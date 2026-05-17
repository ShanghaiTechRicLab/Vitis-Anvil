"""Unit tests for Vitis link artifact parsing."""
from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from hlsflow.parse_link import parse_link_artifacts, parse_link_config  # noqa: E402


@pytest.mark.fast
def test_parse_link_config_extracts_connectivity(tmp_path: Path) -> None:
    cfg = tmp_path / "link.cfg"
    cfg.write_text(
        """
[connectivity]
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]

[clock]
freqHz=300000000:saxpy_1
""".strip(),
        encoding="utf-8",
    )

    cus, mem, clocks = parse_link_config(cfg)

    assert [(cu.kernel, cu.count, cu.name) for cu in cus] == [
        ("saxpy", 1, "saxpy_1"),
        ("vadd", 1, "vadd_1"),
    ]
    assert [(m.endpoint, m.memory) for m in mem] == [
        ("saxpy_1.x", "DDR[0]"),
        ("saxpy_1.y", "DDR[1]"),
        ("saxpy_1.out", "DDR[2]"),
    ]
    assert [(c.endpoint, c.freq_hz) for c in clocks] == [("saxpy_1", 300000000)]


@pytest.mark.fast
def test_parse_link_artifacts_collects_xclbin_logs_and_cfg(tmp_path: Path) -> None:
    build = tmp_path / "build" / "u250-host"
    link_dir = build / "src" / "kernels" / "saxpy_xclbin" / "link" / "run_link"
    link_dir.mkdir(parents=True)
    (build / "src" / "kernels" / "saxpy_xclbin" / "saxpy.xclbin").write_bytes(b"xclbin")
    (build / "src" / "kernels" / "saxpy_xclbin" / "link.cfg").write_text(
        "nk=saxpy:1:saxpy_1\nsp=saxpy_1.x:DDR[0]\nfreqHz=300000000:saxpy_1\n",
        encoding="utf-8",
    )
    (link_dir / "v++.log").write_text(
        "v++ --link --target hw --platform /opt/xilinx/platforms/u250.xpfm\n"
        "WARNING: [v++ 60-123] synthetic warning\n",
        encoding="utf-8",
    )

    rpt = parse_link_artifacts(build, platform="u250")

    assert rpt.status == "pass"
    assert [p.name for p in rpt.xclbins] == ["saxpy.xclbin"]
    assert rpt.target == "hw"
    assert rpt.warnings and "synthetic warning" in rpt.warnings[0]
    assert [(cu.kernel, cu.name) for cu in rpt.compute_units] == [("saxpy", "saxpy_1")]
    assert [(m.endpoint, m.memory) for m in rpt.memory_connections] == [("saxpy_1.x", "DDR[0]")]


@pytest.mark.fast
def test_parse_link_artifacts_marks_vpp_errors_as_fail(tmp_path: Path) -> None:
    build = tmp_path / "build" / "u250-host"
    link_dir = build / "src" / "kernels" / "saxpy_xclbin" / "link" / "run_link"
    link_dir.mkdir(parents=True)
    (link_dir / "v++.log").write_text(
        "ERROR: [v++ 60-661] v++ link run 'run_link' failed\n",
        encoding="utf-8",
    )

    rpt = parse_link_artifacts(build, platform="u250")

    assert rpt.status == "fail"
    assert rpt.errors and "run_link" in rpt.errors[0]
