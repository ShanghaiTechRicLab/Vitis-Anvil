"""Rich rendering of CsynthReport — console + HTML + TXT exports."""
from __future__ import annotations

from pathlib import Path
import io

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.tree import Tree

from hlsflow.parse_csynth import CsynthReport
from hlsflow.platform_info import get_platform_info




def _mhz(period_ns: float | None) -> str:
    if not period_ns or period_ns <= 0:
        return "?"
    return f"{1000.0 / period_ns:,.1f} MHz"


def _latency_time(cycles: int | None, period_ns: float | None) -> str:
    if cycles is None or not period_ns:
        return "?"
    ns = cycles * period_ns
    if ns >= 1_000_000:
        return f"{ns / 1_000_000:.3f} ms"
    if ns >= 1_000:
        return f"{ns / 1_000:.3f} us"
    return f"{ns:.1f} ns"


def _timing_text(rpt: CsynthReport) -> Text:
    if rpt.target_clock_ns is None or rpt.estimated_clock_ns is None:
        return Text("timing=?", style="yellow")
    slack = rpt.target_clock_ns - rpt.estimated_clock_ns
    pct = slack / rpt.target_clock_ns * 100.0 if rpt.target_clock_ns else 0.0
    style = "green" if slack >= 0 else "red"
    return Text(f"slack={slack:+.3f} ns ({pct:+.1f}%)", style=style)


def _build_device_table(rpt: CsynthReport, platform: str) -> Table:
    info = get_platform_info(platform)
    table = Table(show_header=False, box=None, pad_edge=False)
    table.add_column("Key", style="bold")
    table.add_column("Value")
    table.add_row("platform", platform)
    table.add_row("part", rpt.part or (info.part if info else "?"))
    table.add_row("family", rpt.family or (info.family if info else "?"))
    if info and info.default_clock_mhz:
        table.add_row("default clock", f"{info.default_clock_mhz} MHz (platform config)")
    if info:
        table.add_row("memory", info.memory)
        if info.notes:
            table.add_row("notes", info.notes)
    return table


def _build_timing_table(rpt: CsynthReport) -> Table:
    table = Table(show_header=True, header_style="bold")
    table.add_column("Metric")
    table.add_column("Value")
    table.add_row("target clock", f"{rpt.target_clock_ns:.3f} ns / {_mhz(rpt.target_clock_ns)}" if rpt.target_clock_ns else "?")
    table.add_row("estimated clock", f"{rpt.estimated_clock_ns:.3f} ns / {_mhz(rpt.estimated_clock_ns)}" if rpt.estimated_clock_ns else "?")
    table.add_row("clock uncertainty", f"{rpt.clock_uncertainty_ns:.3f} ns" if rpt.clock_uncertainty_ns is not None else "?")
    table.add_row("timing", _timing_text(rpt))
    table.add_row("latency min", f"{rpt.latency_min:,} cycles / {_latency_time(rpt.latency_min, rpt.target_clock_ns)}" if rpt.latency_min is not None else "?")
    table.add_row("latency max", f"{rpt.latency_max:,} cycles / {_latency_time(rpt.latency_max, rpt.target_clock_ns)}" if rpt.latency_max is not None else "?")
    table.add_row("interval min/max", f"{rpt.interval_min if rpt.interval_min is not None else '?'} / {rpt.interval_max if rpt.interval_max is not None else '?'} cycles")
    return table

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


def _build_table(rpt: CsynthReport, platform: str) -> Table:
    table = Table(show_header=True, header_style="bold")
    table.add_column("Resource")
    table.add_column("Used", justify="right")
    table.add_column("Total", justify="right")
    table.add_column("Utilization", justify="left")
    info = get_platform_info(platform)
    available = dict(info.resources) if info else {}
    available.update(rpt.available)
    for key in ("LUT", "FF", "DSP", "BRAM_18K", "URAM"):
        used = rpt.resources.get(key)
        total = available.get(key)
        if used is None or total is None or total == 0:
            continue
        util = used / total
        color = _util_color(util)
        cell = Text.assemble(_bar(util) + "  ", (f"{util * 100:5.1f}%", color))
        headroom = total - used
        table.add_row(key, f"{used:,}", f"{total:,}", Text.assemble(cell, f"  free={headroom:,}"))
    return table


def _build_loops(rpt: CsynthReport) -> Tree:
    tree = Tree(rpt.top)
    for loop in rpt.loops:
        ii_txt = _ii_marker(loop.pipeline_ii)
        lat = f"lat={loop.latency:,}" if loop.latency is not None else "lat=?"
        trip = f"trip={loop.trip_count:,}" if loop.trip_count is not None else "trip=?"
        tree.add(Text.assemble(loop.name, "   II=", ii_txt, f"   {trip}   {lat}"))
    return tree




def _build_interface_table(rpt: CsynthReport) -> Table:
    table = Table(show_header=True, header_style="bold")
    table.add_column("Protocol")
    table.add_column("Objects")
    table.add_column("Ports", justify="right")
    table.add_column("Max bits", justify="right")
    grouped: dict[str, dict[str, object]] = {}
    for iface in rpt.interfaces:
        proto = iface.protocol or "?"
        entry = grouped.setdefault(proto, {"objects": set(), "ports": 0, "max_bits": 0})
        entry["objects"].add(iface.obj or iface.name)  # type: ignore[union-attr]
        entry["ports"] = int(entry["ports"]) + 1
        entry["max_bits"] = max(int(entry["max_bits"]), iface.bits or 0)
    for proto, entry in sorted(grouped.items()):
        objects = sorted(str(x) for x in entry["objects"])  # type: ignore[index]
        shown = ", ".join(objects[:6]) + ("…" if len(objects) > 6 else "")
        table.add_row(proto, shown, str(entry["ports"]), str(entry["max_bits"]) if entry["max_bits"] else "?")
    return table

def render(rpt: CsynthReport, *, kernel: str, platform: str, vitis_version: str,
           html_path: Path, txt_path: Path, console: Console | None = None) -> None:
    rec_console = Console(record=True, width=100, file=io.StringIO())
    header = f"HLS Synthesis: {kernel} / {platform}"
    clk = ""
    if rpt.target_clock_ns and rpt.estimated_clock_ns:
        clk = f"  Clock: {rpt.target_clock_ns:.2f} ns target → {rpt.estimated_clock_ns:.2f} ns estimated"
    version = rpt.version or vitis_version
    rec_console.print(Panel(f"Vitis {version}{clk}  {_timing_text(rpt).plain}", title=header, expand=False))
    rec_console.print("DEVICE")
    rec_console.print(_build_device_table(rpt, platform))
    rec_console.print()
    rec_console.print("TIMING / LATENCY")
    rec_console.print(_build_timing_table(rpt))
    rec_console.print()
    lat = rpt.latency_max if rpt.latency_max is not None else "?"
    perf = Text.assemble("PERFORMANCE   II=", _ii_marker(rpt.worst_loop_ii),
                         f"   Latency={lat:,} cycles" if isinstance(lat, int) else "   Latency=?")
    rec_console.print(perf)
    rec_console.print()
    rec_console.print("RESOURCES")
    rec_console.print(_build_table(rpt, platform))
    rec_console.print()
    rec_console.print("LOOPS")
    rec_console.print(_build_loops(rpt))
    if rpt.interfaces:
        rec_console.print()
        rec_console.print("INTERFACES")
        rec_console.print(_build_interface_table(rpt))
    if rpt.violations:
        rec_console.print()
        rec_console.print("VIOLATIONS")
        for v in rpt.violations[:8]:
            rec_console.print(f"- {v}", style="red")

    html_path.parent.mkdir(parents=True, exist_ok=True)
    txt_path.parent.mkdir(parents=True, exist_ok=True)
    rec_console.save_html(str(html_path), clear=False)
    rec_console.save_text(str(txt_path), clear=False)

    live = console or Console()
    live.print(rec_console.export_text(clear=False))
    live.print(f"Saved: {html_path}")
    live.print(f"       {txt_path}")
