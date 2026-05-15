"""Tests for platforms/ board metadata files."""
from __future__ import annotations

from pathlib import Path

import pytest

import anvil.toml as toml

REPO_ROOT = Path(__file__).parent.parent.parent
PLATFORMS = REPO_ROOT / "platforms"


@pytest.mark.fast
def test_zcu102_board_toml_parses():
    data = toml.load_file(PLATFORMS / "zcu102" / "board.toml")
    assert data["board"]["name"] == "zcu102"
    assert data["board"]["device_kind"] == "embedded"
    assert data["board"]["arch"] == "aarch64"
    assert data["board"]["vitis_part"] == "xczu9eg-ffvb1156-2-e"
    assert data["vitis"]["platform"] == "xilinx_zcu102_base_202420_1"
    assert data["vitis"]["clock_hz"] == 200_000_000
    assert data["vitis"]["xclbin_mode"] == "hw_emu"
    assert "HP0" in data["connect"]["hp_ports"]
    assert data["deploy"]["ssh_user"] == "root"
    assert data["deploy"]["deploy_dir"] == "~/anvil-deploy"
    assert data["deploy"]["xrt_setup"] == ". /etc/profile.d/xrt_setup.sh"


@pytest.mark.fast
def test_zcu102_xrt_ini_exists():
    ini = PLATFORMS / "zcu102" / "xrt.ini"
    assert ini.exists(), f"xrt.ini not found: {ini}"
    content = ini.read_text()
    assert "[Runtime]" in content


@pytest.mark.fast
def test_kv260_board_toml_parses():
    data = toml.load_file(PLATFORMS / "kv260" / "board.toml")
    assert data["board"]["name"] == "kv260"
    assert data["board"]["device_kind"] == "embedded"
