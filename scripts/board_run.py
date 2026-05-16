#!/usr/bin/env python3
"""Deploy and run saxpy on a remote board via SSH/SCP.

Usage:
    python scripts/board_run.py --board-ip 192.168.1.100 \
        --xclbin build/zcu102-kernel/src/kernels/saxpy_xclbin/saxpy.xclbin \
        --host-bin build/zcu102-host/src/host/run_saxpy \
        --dataset tiny

Add --dry-run to print commands without executing (no board needed).
"""
from __future__ import annotations

import re
import shlex
import subprocess
import sys
from pathlib import Path

import click

REPO_ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = REPO_ROOT / "python"
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

import anvil.log as log


def _run(cmd: list[str], dry_run: bool) -> None:
    log.info("  $ {}", " ".join(str(c) for c in cmd))
    if not dry_run:
        subprocess.run(cmd, check=True)


def _validate_dataset_name(_ctx: click.Context, _param: click.Parameter, value: str) -> str:
    if value in {".", ".."} or not re.fullmatch(r"[A-Za-z0-9_.-]+", value):
        raise click.BadParameter("dataset must be a simple name: letters, digits, underscore, dot, or dash")
    return value


def _quote_remote_path(path: str) -> str:
    if path.startswith("~/"):
        rest = path[2:]
        return "~/" + shlex.quote(rest)
    return shlex.quote(path)


def _remote_path(deploy_dir: str, *parts: str) -> str:
    return "/".join([deploy_dir.rstrip("/"), *[p.strip("/") for p in parts]])


def _scp_remote(target: str, path: str) -> str:
    return f"{target}:{_quote_remote_path(path)}"


@click.command()
@click.option("--board-ip", required=True, help="Board IP or hostname")
@click.option("--ssh-user", default="root", show_default=True, help="SSH username")
@click.option("--deploy-dir", default="~/anvil-deploy", show_default=True,
              help="Deployment directory on board")
@click.option("--xclbin", required=True, type=click.Path(), help="Local .xclbin path")
@click.option("--host-bin", required=True, type=click.Path(),
              help="Local AArch64 host binary path")
@click.option("--host-app", default="run_saxpy", show_default=True,
              help="Remote host executable name")
@click.option("--xclbin-name", default="saxpy", show_default=True,
              help="Remote xclbin basename without .xclbin")
@click.option("--dataset", default="tiny", show_default=True, callback=_validate_dataset_name,
              help="Dataset name under data/ (safe name: letters, digits, underscore, dot, dash)")
@click.option("--xrt-setup", default=". /etc/profile.d/xrt_setup.sh",
              show_default=True, help="Command to source XRT on board (POSIX dot preferred over bash source)")
@click.option("--dry-run", is_flag=True, default=False,
              help="Print commands without executing (no board needed)")
def main(
    board_ip: str,
    ssh_user: str,
    deploy_dir: str,
    xclbin: str,
    host_bin: str,
    host_app: str,
    xclbin_name: str,
    dataset: str,
    xrt_setup: str,
    dry_run: bool,
) -> None:
    log.init("board_run")
    target = f"{ssh_user}@{board_ip}"
    data_dir = Path("data") / dataset
    out_bin = data_dir / "xrt_hw_out.bin"
    remote_data_dir = _remote_path(deploy_dir, "data", dataset)
    remote_out = _remote_path(remote_data_dir, "xrt_hw_out.bin")

    if dry_run:
        log.info("[DRY RUN] commands will be printed but not executed")

    _run(["ssh", target, f"mkdir -p {_quote_remote_path(remote_data_dir)}"], dry_run)
    remote_host = host_app
    remote_xclbin = f"{xclbin_name}.xclbin"
    _run(["scp", host_bin, _scp_remote(target, _remote_path(deploy_dir, remote_host))], dry_run)
    _run(["scp", xclbin, _scp_remote(target, _remote_path(deploy_dir, remote_xclbin))], dry_run)
    _run(["scp", "-r", f"{data_dir}/.", _scp_remote(target, remote_data_dir + "/")], dry_run)

    remote_cmd = (
        f"{xrt_setup} && "
        f"cd {_quote_remote_path(deploy_dir)} && "
        f"chmod +x {shlex.quote(remote_host)} && "
        f"./{shlex.quote(remote_host)} --xclbin {shlex.quote(remote_xclbin)} "
        f"--data-dir {shlex.quote('data/' + dataset)} "
        f"--output {shlex.quote('data/' + dataset + '/xrt_hw_out.bin')}"
    )
    _run(["ssh", target, remote_cmd], dry_run)
    _run(["scp", _scp_remote(target, remote_out), str(out_bin)], dry_run)

    log.info("board_run: output retrieved to {}", str(out_bin))
    log.info("Run 'make compare DATASET={}' to validate against gold.", dataset)


if __name__ == "__main__":
    main()
