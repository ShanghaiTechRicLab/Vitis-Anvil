"""anvil.json — stdlib json helpers mirroring anvil::json C++ API."""
from __future__ import annotations

import json as _json
from pathlib import Path
from typing import Any


def load_file(path: Path | str) -> Any:
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"anvil.json: file not found: {path}")
    with path.open(encoding="utf-8") as f:
        return _json.load(f)


def dump_file(path: Path | str, obj: Any, indent: int = 2) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        _json.dump(obj, f, indent=indent)
        f.write("\n")
