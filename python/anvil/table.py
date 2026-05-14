"""anvil.table — rich.table wrapper mirroring anvil::table C++ API."""
from __future__ import annotations

from collections.abc import Sequence

from rich.console import Console
from rich.table import Table

_console = Console()


def print_table(rows: Sequence[Sequence[str]]) -> None:
    """Print rows as a rich table; first row is the header."""
    if not rows:
        return
    table = Table(*rows[0], highlight=True)
    for row in rows[1:]:
        table.add_row(*row)
    _console.print(table)
