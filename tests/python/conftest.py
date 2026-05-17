import sys
from pathlib import Path

# Make `src.*` importable when running pytest from project root.
sys.path.insert(0, str(Path(__file__).parent.parent.parent))
# Make `hlsflow.*` CLI helpers importable without requiring PYTHONPATH=tools.
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "tools"))
