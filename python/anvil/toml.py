"""anvil.toml — tomllib wrapper mirroring anvil::toml C++ API."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover
    import tomli as tomllib  # type: ignore[import-not-found,no-redef]


def load_file(path: Path | str) -> dict[str, Any]:
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"anvil.toml: file not found: {path}")
    with path.open("rb") as f:
        return tomllib.load(f)


def require(table: dict[str, Any], key: str) -> Any:
    if key not in table:
        raise KeyError(f"anvil.toml: required key '{key}' not found")
    return table[key]
