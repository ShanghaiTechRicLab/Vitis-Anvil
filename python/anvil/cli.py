"""anvil.cli — click wrapper mirroring anvil::cli C++ API."""
from __future__ import annotations

import sys

import click

command = click.command
option = click.option
argument = click.argument
group = click.group
pass_context = click.pass_context


def error_exit(msg: str) -> None:
    click.echo(f"Error: {msg}", err=True)
    sys.exit(1)
