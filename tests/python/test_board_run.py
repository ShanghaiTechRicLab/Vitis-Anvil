"""Tests for scripts/board_run.py — all run offline with --dry-run."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).parent.parent.parent
BOARD_RUN = REPO_ROOT / "scripts" / "board_run.py"


@pytest.mark.fast
def test_dry_run_prints_ssh_and_scp():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "10.0.0.1",
            "--xclbin", "build/zcu102-kernel/src/kernels/saxpy_xclbin/saxpy.xclbin",
            "--host-bin", "build/zcu102-host/src/host/run_saxpy",
            "--dataset", "tiny",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    combined = result.stdout + result.stderr
    assert result.returncode == 0, f"board_run.py exited {result.returncode}:\n{combined}"
    assert "ssh" in combined
    assert "10.0.0.1" in combined
    assert "saxpy.xclbin" in combined
    assert "run_saxpy" in combined


@pytest.mark.fast
def test_dry_run_uses_correct_defaults():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "192.168.1.50",
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    combined = result.stdout + result.stderr
    assert result.returncode == 0
    assert "root@192.168.1.50" in combined
    assert "anvil-deploy" in combined


@pytest.mark.fast
def test_missing_board_ip_exits_nonzero():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    assert result.returncode != 0


@pytest.mark.fast
def test_dataset_name_rejects_shell_metacharacters():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "10.0.0.1",
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--dataset", "tiny;rm-rf",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    assert result.returncode != 0
    assert "dataset must be a simple name" in (result.stdout + result.stderr)


@pytest.mark.fast
def test_dry_run_quotes_remote_deploy_dir_with_spaces():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "10.0.0.1",
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--deploy-dir", "/tmp/anvil deploy",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    combined = result.stdout + result.stderr
    assert result.returncode == 0, combined
    assert "'/tmp/anvil deploy" in combined


@pytest.mark.fast
def test_default_deploy_dir_keeps_remote_tilde_expansion():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "10.0.0.1",
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    combined = result.stdout + result.stderr
    assert result.returncode == 0, combined
    assert "mkdir -p ~/anvil-deploy/data/tiny" in combined
    assert "cd ~/anvil-deploy" in combined
    assert "'~/anvil-deploy" not in combined


@pytest.mark.fast
def test_dataset_name_rejects_dot_segments():
    result = subprocess.run(
        [
            sys.executable, str(BOARD_RUN),
            "--board-ip", "10.0.0.1",
            "--xclbin", "fake.xclbin",
            "--host-bin", "fake_bin",
            "--dataset", "..",
            "--dry-run",
        ],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    assert result.returncode != 0
