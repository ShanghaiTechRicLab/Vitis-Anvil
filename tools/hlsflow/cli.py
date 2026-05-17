"""hlsflow CLI command group (Click)."""
from __future__ import annotations

import os
import subprocess
import csv
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

import click

from hlsflow import __version__
from rich.console import Console
from rich.table import Table
from rich.markup import escape

from hlsflow.check import run_checks
from hlsflow.compare import render_diff
from hlsflow.compare_gold import compare_gold_hw
from hlsflow.database import RunRecord, append as db_append, find_by_id, latest, load_all, now_run_id
from hlsflow.discover import find_cosim_dir, find_csynth_for_kernel, find_csynth_reports
from hlsflow.parse_cosim import parse_cosim_dir
from hlsflow.parse_csynth import parse_csynth_report
from hlsflow.parse_link import LinkReport, parse_link_artifacts
from hlsflow.parse_vitis import find_and_parse_logs
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
    vlog = find_and_parse_logs(hit.work_dir, kernel)
    v_errors = [e for lr in vlog for e in lr.errors]
    v_warnings = [w for lr in vlog for w in lr.warnings]
    v_timing = [t for lr in vlog for t in lr.timing_violations]
    if v_errors:
        console.print(f"[red]vitis errors ({len(v_errors)})[/red]: {v_errors[0]}")
    elif v_warnings:
        console.print(f"[yellow]vitis warnings ({len(v_warnings)})[/yellow]: {v_warnings[0]}")
    rec = RunRecord(
        run_id=run_id, kernel=kernel, platform=platform, target="csynth",
        git_commit=_git_commit(), build_dir=str(build_dir), vitis_version=vitis_v,
        status="pass", timestamp=datetime.now(timezone.utc).isoformat(),
        reports={"csynth_xml": str(hit.report_path), "html": str(html), "txt": str(txt)},
        metrics={
            "report_version": rpt.version,
            "part": rpt.part,
            "family": rpt.family,
            "clock_uncertainty_ns": rpt.clock_uncertainty_ns,
            "target_clock_ns": rpt.target_clock_ns,
            "estimated_clock_ns": rpt.estimated_clock_ns,
            "timing_slack_ns": (rpt.target_clock_ns - rpt.estimated_clock_ns) if (rpt.target_clock_ns is not None and rpt.estimated_clock_ns is not None) else None,
            "timing_met": rpt.timing_met,
            "latency_min": rpt.latency_min,
            "latency_max": rpt.latency_max,
            "interval_min": rpt.interval_min,
            "interval_max": rpt.interval_max,
            "worst_loop_ii": rpt.worst_loop_ii,
            **{k.lower(): v for k, v in rpt.resources.items()},
            **{f"{k.lower()}_avail": v for k, v in rpt.available.items()},
            "vitis_errors": v_errors[:5],
            "vitis_warnings": v_warnings[:5],
            "vitis_timing_violations": v_timing[:5],
        },
    )
    db_append(rec, reports_dir / "runs.jsonl")
    return rec


def _fmt_cycles(value: int | None) -> str:
    return "NA" if value is None else f"{value:,}"


def _render_cosim(cr, kernel: str, platform: str, console: Console) -> None:
    status_style = "green" if cr.status == "pass" else ("red" if cr.status == "fail" else "yellow")
    tool = cr.sim_tool or "unknown-sim"
    console.print(f"[bold]COSIM {kernel}/{platform}[/bold]  status=[{status_style}]{cr.status}[/{status_style}]  tool={tool}")

    if cr.rtl_results:
        table = Table(title="RTL cosim summary", show_header=True, header_style="bold")
        table.add_column("RTL")
        table.add_column("Status")
        table.add_column("Latency min/avg/max", justify="right")
        table.add_column("Interval min/avg/max", justify="right")
        table.add_column("Total", justify="right")
        for row in cr.rtl_results:
            style = "green" if row.status.lower() == "pass" else ("dim" if row.status.upper() == "NA" else "red")
            table.add_row(
                row.rtl,
                f"[{style}]{row.status}[/{style}]",
                f"{_fmt_cycles(row.latency_min)}/{_fmt_cycles(row.latency_avg)}/{_fmt_cycles(row.latency_max)}",
                f"{_fmt_cycles(row.interval_min)}/{_fmt_cycles(row.interval_avg)}/{_fmt_cycles(row.interval_max)}",
                _fmt_cycles(row.total_cycles),
            )
        console.print(table)
    else:
        console.print(f"  latency={_fmt_cycles(cr.latency_cycles)} cycles")

    if cr.transactions:
        latencies = [t.latency_cycles for t in cr.transactions]
        intervals = [t.interval_cycles for t in cr.transactions if t.interval_cycles is not None]
        interval_text = "NA" if not intervals else f"{min(intervals):,}/{max(intervals):,}"
        console.print(
            f"  transactions={len(cr.transactions)}  "
            f"latency_min/max={min(latencies):,}/{max(latencies):,}  "
            f"interval_min/max={interval_text}"
        )
    elif cr.lat_summary:
        console.print(
            "  lat.rpt: "
            f"min={_fmt_cycles(cr.lat_summary.get('min_latency'))} "
            f"avg={_fmt_cycles(cr.lat_summary.get('aver_latency'))} "
            f"max={_fmt_cycles(cr.lat_summary.get('max_latency'))} "
            f"total={_fmt_cycles(cr.lat_summary.get('total_execute_time'))}"
        )

    preferred_messages = sorted(
        dict.fromkeys(cr.messages),
        key=lambda msg: ("COSIM-1000" not in msg, "finished" not in msg.lower(), msg),
    )
    for msg in preferred_messages[:3]:
        console.print(f"  [dim]{msg}[/dim]")
    rel_files = [str(p.relative_to(cr.report_dir)) for p in cr.found_files]
    console.print(f"  artifacts: {', '.join(rel_files[:6])}" + (" ..." if len(rel_files) > 6 else ""))


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
    _render_cosim(cr, kernel, platform, console)
    vitis_v = _vitis_version()
    pass_row = cr.passing_rtl
    metrics = {
        "cosim_status": cr.status,
        "cosim_latency_cycles": cr.latency_cycles,
        "cosim_tool": cr.sim_tool,
        "cosim_solution": cr.solution,
        "cosim_transaction_count": cr.transaction_count,
        "cosim_total_cycles": pass_row.total_cycles if pass_row else None,
        "cosim_interval_min": pass_row.interval_min if pass_row else None,
        "cosim_interval_max": pass_row.interval_max if pass_row else None,
    }
    rec = RunRecord(
        run_id=run_id, kernel=kernel, platform=platform, target="cosim",
        git_commit=_git_commit(), build_dir=str(build_dir), vitis_version=vitis_v,
        status=cr.status, timestamp=datetime.now(timezone.utc).isoformat(),
        reports={"cosim_dir": str(sim_dir)},
        metrics=metrics,
    )
    db_append(rec, reports_dir / "runs.jsonl")
    return rec


def _fmt_freq(freq_hz: int | None) -> str:
    if freq_hz is None:
        return "?"
    if freq_hz % 1_000_000 == 0:
        return f"{freq_hz // 1_000_000} MHz"
    return f"{freq_hz:,} Hz"


def _render_link(lr: LinkReport, name: str, platform: str, console: Console) -> None:
    status_style = "green" if lr.status == "pass" else ("red" if lr.status == "fail" else "yellow")
    console.print(f"[bold]LINK {name}/{platform}[/bold]  status=[{status_style}]{lr.status}[/{status_style}]  target={lr.target or '?'}")

    if lr.xclbins:
        table = Table(title="xclbin outputs", show_header=True, header_style="bold")
        table.add_column("File")
        table.add_column("Size", justify="right")
        for xclbin in lr.xclbins:
            try:
                size = f"{xclbin.stat().st_size:,} B"
            except OSError:
                size = "?"
            table.add_row(str(xclbin), size)
        console.print(table)
    else:
        console.print("  [yellow]no .xclbin found under build dir[/yellow]")

    if lr.compute_units:
        table = Table(title="compute units", show_header=True, header_style="bold")
        table.add_column("Kernel")
        table.add_column("Count", justify="right")
        table.add_column("CU name")
        for cu in lr.compute_units:
            table.add_row(cu.kernel, str(cu.count), cu.name)
        console.print(table)

    if lr.memory_connections:
        table = Table(title="memory connectivity", show_header=True, header_style="bold")
        table.add_column("Endpoint")
        table.add_column("Memory")
        for conn in lr.memory_connections:
            table.add_row(conn.endpoint, conn.memory)
        console.print(table)

    if lr.clocks:
        table = Table(title="clock settings", show_header=True, header_style="bold")
        table.add_column("Endpoint")
        table.add_column("Frequency", justify="right")
        for clk in lr.clocks:
            table.add_row(clk.endpoint, _fmt_freq(clk.freq_hz))
        console.print(table)

    if lr.errors:
        console.print(f"[red]vitis link errors ({len(lr.errors)})[/red]: {escape(lr.errors[0])}")
    elif lr.warnings:
        console.print(f"[yellow]vitis link warnings ({len(lr.warnings)})[/yellow]: {escape(lr.warnings[0])}")

    artifacts = [str(p) for p in lr.config_files + [Path(log.log_path) for log in lr.logs]]
    if artifacts:
        console.print(f"  artifacts: {', '.join(artifacts[:8])}" + (" ..." if len(artifacts) > 8 else ""))


def _do_collect_link(build_dir: Path, name: str, platform: str, reports_dir: Path,
                     console: Console) -> RunRecord | None:
    lr = parse_link_artifacts(build_dir, platform=platform)
    _render_link(lr, name, platform, console)
    vitis_v = _vitis_version()
    run_id = now_run_id(name, platform)
    rec = RunRecord(
        run_id=run_id, kernel=name, platform=platform, target="link",
        git_commit=_git_commit(), build_dir=str(build_dir), vitis_version=vitis_v,
        status=lr.status, timestamp=datetime.now(timezone.utc).isoformat(),
        reports={
            "xclbins": ";".join(str(p) for p in lr.xclbins),
            "logs": ";".join(log.log_path for log in lr.logs),
            "link_cfg": ";".join(str(p) for p in lr.config_files),
        },
        metrics={
            "xclbin_count": len(lr.xclbins),
            "xclbins": [str(p) for p in lr.xclbins],
            "link_target": lr.target,
            "link_platform": lr.platform,
            "compute_units": [cu.__dict__ for cu in lr.compute_units],
            "memory_connections": [conn.__dict__ for conn in lr.memory_connections],
            "clocks": [clk.__dict__ for clk in lr.clocks],
            "vitis_errors": lr.errors[:5],
            "vitis_warnings": lr.warnings[:5],
        },
    )
    db_append(rec, reports_dir / "runs.jsonl")
    return rec


@cli.command()
@click.option("--build-dir", required=True, type=click.Path(exists=True, file_okay=False, path_type=Path))
@click.option("--kernel", required=True, help="Kernel name, or 'all' to scan every kernel")
@click.option("--target", type=click.Choice(["csynth", "cosim", "link"]), default="csynth")
@click.option("--platform", default=None, help="Override platform tag (default: inferred from build-dir name)")
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
def collect(build_dir: Path, kernel: str, target: str, platform: str | None, reports_dir: Path) -> None:
    """Parse an HLS report, render rich output, append to reports/runs.jsonl."""
    console = Console()
    platform_tag = _platform_from_build_dir(build_dir, platform)
    reports_dir.mkdir(parents=True, exist_ok=True)
    if target == "link":
        name = "xclbin" if kernel == "all" else kernel
        rec = _do_collect_link(build_dir, name, platform_tag, reports_dir, console)
        raise SystemExit(1 if rec is None or rec.status == "fail" else 0)

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
@click.option("--target", type=click.Choice(["csynth", "cosim", "link"]), default="csynth")
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


@cli.command()
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
@click.option("--output", "-o", default=None, type=click.Path(path_type=Path),
              help="Output CSV path (default: reports/runs.csv)")
def export(reports_dir: Path, output: Path | None) -> None:
    """Export runs.jsonl to a flat CSV (metrics become columns)."""
    store = reports_dir / "runs.jsonl"
    records = load_all(store)
    if not records:
        click.echo("No records found.", err=True)
        raise SystemExit(0)

    base_cols = ["run_id", "kernel", "platform", "target",
                 "git_commit", "build_dir", "vitis_version", "status", "timestamp"]
    metric_keys: list[str] = []
    for rec in records:
        for k in rec.metrics:
            if k not in metric_keys:
                metric_keys.append(k)

    out_path = output or (reports_dir / "runs.csv")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=base_cols + metric_keys, extrasaction="ignore")
        writer.writeheader()
        for rec in records:
            row = {col: getattr(rec, col, "") for col in base_cols}
            row.update(rec.metrics)
            writer.writerow(row)

    click.echo(f"Exported {len(records)} records → {out_path}")


@cli.command()
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
@click.option("--output", "-o", default=None, type=click.Path(path_type=Path),
              help="Output Markdown path (default: reports/summary.md)")
def summary(reports_dir: Path, output: Path | None) -> None:
    """Generate summary.md — latest run per kernel/platform/target."""
    records = load_all(reports_dir / "runs.jsonl")
    if not records:
        click.echo("No records found.", err=True)
        raise SystemExit(0)

    # last record per (kernel, platform, target) wins (JSONL is append-only)
    latest_records: dict[tuple[str, str, str], RunRecord] = {}
    for rec in records:
        latest_records[(rec.kernel, rec.platform, rec.target)] = rec

    csynth_cols = ["kernel", "platform", "status", "worst_loop_ii", "latency_max",
                   "lut", "ff", "dsp", "bram_18k", "uram", "estimated_clock_ns", "timing_met"]
    cosim_cols = ["kernel", "platform", "status", "cosim_status", "cosim_latency_cycles"]
    hw_cols = ["kernel", "platform", "status", "mae", "rms", "tol"]

    def _md_row(rec: RunRecord, cols: list[str]) -> str:
        vals = []
        for c in cols:
            v = getattr(rec, c, None)
            if v is None:
                v = rec.metrics.get(c, "—")
            vals.append(str(v) if v not in (None, "") else "—")
        return "| " + " | ".join(vals) + " |"

    def _md_table(recs: list[RunRecord], cols: list[str]) -> str:
        header = "| " + " | ".join(cols) + " |"
        sep = "| " + " | ".join(["---"] * len(cols)) + " |"
        return "\n".join([header, sep] + [_md_row(r, cols) for r in recs]) + "\n"

    by_target: dict[str, list[RunRecord]] = defaultdict(list)
    for rec in sorted(latest_records.values(), key=lambda r: (r.kernel, r.platform)):
        by_target[rec.target].append(rec)

    lines = [f"# HLS Flow Summary\n\n_Source: `{reports_dir}/runs.jsonl`_\n\n"]
    target_cols = {"csynth": csynth_cols, "cosim": cosim_cols, "hw": hw_cols}
    for target in sorted(by_target):
        cols = target_cols.get(target, ["kernel", "platform", "status"])
        lines.append(f"## {target.upper()}\n\n")
        lines.append(_md_table(by_target[target], cols))
        lines.append("\n")

    out_path = output or (reports_dir / "summary.md")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("".join(lines), encoding="utf-8")
    click.echo(f"Summary → {out_path}")


@cli.command("compare-gold")
@click.option("--dataset", required=True, help="Dataset name under data/ (e.g. tiny)")
@click.option("--kernel", required=True, help="Kernel name (e.g. saxpy)")
@click.option("--platform", required=True, help="Platform tag (e.g. u250)")
@click.option("--tol", default=1e-5, type=float,
              help="Max-abs-error pass threshold (default: 1e-5)")
@click.option("--reports-dir", default="reports", type=click.Path(path_type=Path))
@click.option("--data-dir", default="data", type=click.Path(path_type=Path))
def compare_gold_cmd(dataset: str, kernel: str, platform: str, tol: float,
                     reports_dir: Path, data_dir: Path) -> None:
    """Compare gold_out.bin vs xrt_hw_out.bin; append hw record to runs.jsonl."""
    console = Console()
    try:
        rec = compare_gold_hw(
            data_dir / dataset, kernel, platform, tol=tol,
            git_commit=_git_commit(), vitis_version=_vitis_version(),
        )
    except (FileNotFoundError, ValueError) as exc:
        console.print(f"[red]error[/red]: {exc}")
        raise SystemExit(2)

    color = "green" if rec.status == "pass" else "red"
    console.print(
        f"[{color}]{rec.status.upper()}[/{color}]  {kernel}/{platform}  "
        f"mae={rec.metrics['mae']:.3e}  rms={rec.metrics['rms']:.3e}  (tol={tol:.0e})"
    )
    reports_dir.mkdir(parents=True, exist_ok=True)
    db_append(rec, reports_dir / "runs.jsonl")
    console.print(f"  appended run_id={rec.run_id}")
    raise SystemExit(0 if rec.status == "pass" else 1)
