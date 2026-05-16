"""Diff two RunRecord metric dicts."""
from __future__ import annotations

from rich.console import Console
from rich.table import Table

from hlsflow.database import RunRecord


def _delta(a: float | int | None, b: float | int | None) -> str:
    if a is None or b is None:
        return "—"
    d = b - a
    sign = "+" if d > 0 else ""
    return f"{sign}{d}"


def render_diff(baseline: RunRecord, candidate: RunRecord, console: Console) -> None:
    table = Table(title=f"{baseline.run_id} → {candidate.run_id}", show_header=True)
    table.add_column("Metric")
    table.add_column("Baseline", justify="right")
    table.add_column("Candidate", justify="right")
    table.add_column("Delta", justify="right")
    keys = sorted(set(baseline.metrics.keys()) | set(candidate.metrics.keys()))
    worse_keys = {"worst_loop_ii", "lut", "ff", "dsp", "bram_18k", "uram", "latency_max"}
    for key in keys:
        a = baseline.metrics.get(key)
        b = candidate.metrics.get(key)
        delta = _delta(a, b) if isinstance(a, (int, float)) and isinstance(b, (int, float)) else "—"
        style = ""
        if isinstance(a, (int, float)) and isinstance(b, (int, float)) and key in worse_keys:
            if b > a:
                style = "red"
            elif b < a:
                style = "green"
        table.add_row(key, str(a), str(b), f"[{style}]{delta}[/{style}]" if style else delta)
    console.print(table)
