"""hlsflow CLI command group (Click)."""
from __future__ import annotations

import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import click

from hlsflow import __version__
from rich.console import Console

from hlsflow.check import run_checks
from hlsflow.compare import render_diff
from hlsflow.database import RunRecord, append as db_append, find_by_id, latest, now_run_id
from hlsflow.discover import find_cosim_dir, find_csynth_for_kernel, find_csynth_reports
from hlsflow.parse_cosim import parse_cosim_dir
from hlsflow.parse_csynth import parse_csynth_report
from hlsflow.report_md import render


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
@click.version_option(version=__version__, prog_name="hlsflow")
def cli() -> None:
    """Vitis-Anvil HLS report analysis."""


def _git_commit() -> str:
    try:
        out = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], stderr=subprocess.DEVNULL)
        return out.decode().strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def _vitis_version() -> str:
    return os.environ.get("XILINX_VITIS", "unknown").rstrip("/").rsplit("/", 1)[-1] or "unknown"


def _platform_from_build_dir(build_dir: Path, override: str | None) -> str:
    if override:
        return override
    name = build_dir.name
    for known in ("u250", "u280", "u55c", "zcu102", "zcu104", "kv260"):
        if known in name:
            return known
    return "unknown"


def _do_collect_csynth(build_dir: Path, kernel: str, platform: str, reports_dir: Path,
                       console: Console) -> RunRecord | None:
    hit = find_csynth_for_kernel(build_dir, kernel)
    if hit is None:
        console.print(f"[red]error[/red]: csynth report for kernel '{kernel}' not found under {build_dir}")
        return None
    rpt = parse_csynth_report(hit.report_path)
    run_id = now_run_id(kernel, platform)
    html = reports_dir / f"{run_id}.html"
    txt = reports_dir / f"{run_id}.txt"
    vitis_v = _vitis_version()
    render(rpt, kernel=kernel, platform=platform, vitis_version=vitis_v,
           html_path=html, txt_path=txt, console=console)
    rec = RunRecord(
        run_id=run_id, kernel=kernel, platform=platform, target="csynth",
        git_commit=_git_commit(), build_dir=str(build_dir), vitis_version=vitis_v,
        status="pass", timestamp=datetime.now(timezone.utc).isoformat(),
        reports={"csynth_xml": str(hit.report_path), "html": str(html), "txt": str(txt)},
        metrics={
            "target_clock_ns": rpt.target_clock_ns,
            "estimated_clock_ns": rpt.estimated_clock_ns,
            "timing_met": rpt.timing_met,
            "latency_min": rpt.latency_min,
            "latency_max": rpt.latency_max,
            "worst_loop_ii": rpt.worst_loop_ii,
            **{k.lower(): v for k, v in rpt.resources.items()},
            **{f"{k.lower()}_avail": v for k, v in rpt.available.items()},
        },
    )
    db_append(rec, reports_dir / "runs.jsonl")
    return rec


def _do_collect_cosim(build_dir: Path, kernel: str, platform: str, reports_dir: Path,
                      console: Console) -> RunRecord | None:
    hit = find_csynth_for_kernel(build_dir, kernel)
    if hit is None:
        console.print(f"[red]error[/red]: kernel '{kernel}' work_dir not found under {build_dir}")
        return None
    sim_dir = find_cosim_dir(hit.work_dir)
    if sim_dir is None:
        console.print(f"[yellow]warn[/yellow]: no cosim report dir under {hit.work_dir}; run `make cosim` first")
        return None
    cr = parse_cosim_dir(sim_dir)
    run_id = now_run_id(kernel, platform)
    console.print(f"COSIM {kernel}/{platform}  status={cr.status}  latency={cr.latency_cycles}")
    console.print(f"  files: {[str(p.relative_to(sim_dir)) for p in cr.found_files]}")
    vitis_v = _vitis_version()
    rec = RunRecord(
        run_id=run_id, kernel=kernel, platform=platform, target="cosim",
        git_commit=_git_commit(), build_dir=str(build_dir), vitis_version=vitis_v,
        status=cr.status, timestamp=datetime.now(timezone.utc).isoformat(),
        reports={"cosim_dir": str(sim_dir)},
        metrics={"cosim_status": cr.status, "cosim_latency_cycles": cr.latency_cycles},
    )
    db_append(rec, reports_dir / "runs.jsonl")
    return rec


@cli.command()
@click.option("--build-dir", required=True, type=click.Path(exists=True, file_okay=False, path_type=Path))
@click.option("--kernel", required=True, help="Kernel name, or 'all' to scan every kernel")
@click.option("--target", type=click.Choice(["csynth", "cosim"]), default="csynth")
@click.option("--platform", default=None, help="Override platform tag (default: inferred from build-dir name)")
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
def collect(build_dir: Path, kernel: str, target: str, platform: str | None, reports_dir: Path) -> None:
    """Parse an HLS report, render rich output, append to reports/runs.jsonl."""
    console = Console()
    platform_tag = _platform_from_build_dir(build_dir, platform)
    reports_dir.mkdir(parents=True, exist_ok=True)
    kernels = [h.kernel for h in find_csynth_reports(build_dir)] if kernel == "all" else [kernel]
    if not kernels:
        console.print(f"[red]error[/red]: no csynth reports found under {build_dir}")
        raise SystemExit(2)
    fn = _do_collect_csynth if target == "csynth" else _do_collect_cosim
    any_fail = False
    for k in kernels:
        rec = fn(build_dir, k, platform_tag, reports_dir, console)
        if rec is None or (target == "cosim" and rec.status == "fail"):
            any_fail = True
    raise SystemExit(1 if any_fail else 0)


@cli.command()
@click.option("--run-id", default=None)
@click.option("--kernel", default=None, help="Filter latest by kernel")
@click.option("--target", type=click.Choice(["csynth", "cosim"]), default="csynth")
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
@click.option("--max-ii", type=int, default=None)
@click.option("--max-lut", type=int, default=None)
@click.option("--max-ff", type=int, default=None)
@click.option("--max-dsp", type=int, default=None)
@click.option("--max-bram", type=int, default=None)
@click.option("--max-uram", type=int, default=None)
@click.option("--max-latency", type=int, default=None)
@click.option("--min-latency", type=int, default=None)
def check(run_id: str | None, kernel: str | None, target: str, reports_dir: Path,
          **thresholds: int | None) -> None:
    """Assert resource/timing thresholds against a stored run record."""
    console = Console()
    store = reports_dir / "runs.jsonl"
    rec = find_by_id(run_id, store) if run_id else latest(store, kernel=kernel, target=target)
    if rec is None:
        console.print(f"[red]error[/red]: no run record found (run_id={run_id} kernel={kernel} target={target})")
        raise SystemExit(2)
    results = run_checks(rec, **thresholds)
    if not results:
        console.print(f"[yellow]warn[/yellow]: no thresholds specified for run {rec.run_id}")
        raise SystemExit(0)
    failed = False
    for r in results:
        console.print(f"[{'green' if r.passed else 'red'}]{r.format()}[/{'green' if r.passed else 'red'}]")
        failed = failed or not r.passed
    raise SystemExit(1 if failed else 0)


@cli.command()
@click.option("--baseline", required=True, help="run_id of baseline")
@click.option("--candidate", required=True, help="run_id of candidate")
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
def compare(baseline: str, candidate: str, reports_dir: Path) -> None:
    """Diff two stored run records."""
    console = Console()
    store = reports_dir / "runs.jsonl"
    b = find_by_id(baseline, store)
    c = find_by_id(candidate, store)
    if b is None or c is None:
        console.print(f"[red]error[/red]: baseline={b is not None} candidate={c is not None}")
        raise SystemExit(2)
    render_diff(b, c, console)
