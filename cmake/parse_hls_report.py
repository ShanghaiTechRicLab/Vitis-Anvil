#!/usr/bin/env python3
"""Thin wrapper kept for ctest compatibility."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent
_TOOLS = _REPO_ROOT / "tools"
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from hlsflow.parse_csynth import legacy_cli  # noqa: E402

if __name__ == "__main__":
    sys.exit(legacy_cli(sys.argv[1:]))
