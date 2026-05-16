"""Rich rendering of CsynthReport — console + HTML + TXT exports."""
from __future__ import annotations

from pathlib import Path

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.tree import Tree

from hlsflow.parse_csynth import CsynthReport


def _util_color(util: float) -> str:
    if util > 0.80:
        return "red"
    if util > 0.60:
        return "yellow"
    return "green"


def _bar(util: float, width: int = 15) -> str:
    filled = max(0, min(width, round(util * width)))
    return "▓" * filled + "░" * (width - filled)


def _ii_marker(ii: int | None) -> Text:
    if ii is None:
        return Text("?", style="yellow")
    if ii <= 1:
        return Text(f"✅{ii}", style="green")
    if ii <= 2:
        return Text(f"⚠ {ii}", style="yellow")
    return Text(f"❌{ii}", style="red")


def _build_table(rpt: CsynthReport) -> Table:
    table = Table(show_header=True, header_style="bold")
    table.add_column("Resource")
    table.add_column("Used", justify="right")
    table.add_column("Total", justify="right")
    table.add_column("Utilization", justify="left")
    for key in ("LUT", "FF", "DSP", "BRAM_18K", "URAM"):
        used = rpt.resources.get(key)
        total = rpt.available.get(key)
        if used is None or total is None or total == 0:
            continue
        util = used / total
        color = _util_color(util)
        cell = Text.assemble(_bar(util) + "  ", (f"{util * 100:5.1f}%", color))
        table.add_row(key, f"{used:,}", f"{total:,}", cell)
    return table


def _build_loops(rpt: CsynthReport) -> Tree:
    tree = Tree(rpt.top)
    for loop in rpt.loops:
        ii_txt = _ii_marker(loop.pipeline_ii)
        lat = f"lat={loop.latency:,}" if loop.latency is not None else "lat=?"
        trip = f"trip={loop.trip_count:,}" if loop.trip_count is not None else "trip=?"
        tree.add(Text.assemble(loop.name, "   II=", ii_txt, f"   {trip}   {lat}"))
    return tree


def render(rpt: CsynthReport, *, kernel: str, platform: str, vitis_version: str,
           html_path: Path, txt_path: Path, console: Console | None = None) -> None:
    rec_console = Console(record=True, width=100)
    header = f"HLS Synthesis: {kernel} / {platform}"
    clk = ""
    if rpt.target_clock_ns and rpt.estimated_clock_ns:
        clk = f"  Clock: {rpt.target_clock_ns:.2f} ns target → {rpt.estimated_clock_ns:.2f} ns estimated"
    rec_console.print(Panel(f"Vitis {vitis_version}{clk}", title=header, expand=False))
    lat = rpt.latency_max if rpt.latency_max is not None else "?"
    perf = Text.assemble("PERFORMANCE   II=", _ii_marker(rpt.worst_loop_ii),
                         f"   Latency={lat:,} cycles" if isinstance(lat, int) else "   Latency=?")
    rec_console.print(perf)
    rec_console.print()
    rec_console.print("RESOURCES")
    rec_console.print(_build_table(rpt))
    rec_console.print()
    rec_console.print("LOOPS")
    rec_console.print(_build_loops(rpt))

    html_path.parent.mkdir(parents=True, exist_ok=True)
    txt_path.parent.mkdir(parents=True, exist_ok=True)
    rec_console.save_html(str(html_path), clear=False)
    rec_console.save_text(str(txt_path), clear=False)

    live = console or Console()
    live.print(rec_console.export_text(clear=False))
    live.print(f"Saved: {html_path}")
    live.print(f"       {txt_path}")
