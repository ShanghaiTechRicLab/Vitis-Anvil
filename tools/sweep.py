#!/usr/bin/env python3
"""DSE parameter sweep: runs cmake configure + csynth + hlsflow collect for each point.

Usage:
  python tools/sweep.py sweep.toml [--dry-run] [--reports-dir reports]

TOML config format:

  [sweep]
  target = "u250"
  preset = "u250-host"   # optional; defaults to "<target>-host"
  kernel = "saxpy"

  [sweep.points]
  clock = [200, 250, 300]   # each value → -DANVIL_CLOCK_MHZ=N

All axes in [sweep.points] form a Cartesian product.
"""
from __future__ import annotations

import argparse
import itertools
import subprocess
import sys
from pathlib import Path
from typing import Any

try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib  # type: ignore[import-not-found,no-redef]
    except ImportError:
        print("error: tomllib (stdlib ≥3.11) or tomli required", file=sys.stderr)
        sys.exit(1)


def _cartesian(points: dict[str, Any]) -> list[dict[str, Any]]:
    if not points:
        return [{}]
    keys = list(points.keys())
    vals = [v if isinstance(v, list) else [v] for v in points.values()]
    return [dict(zip(keys, combo)) for combo in itertools.product(*vals)]


def _build_dir(target: str, point: dict[str, Any]) -> str:
    suffix = "_".join(f"{k}{v}" for k, v in sorted(point.items()))
    return f"build/{target}-sweep" + (f"_{suffix}" if suffix else "")


def _run(cmd: list[str], dry: bool) -> int:
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    if dry:
        return 0
    return subprocess.run(cmd).returncode


def sweep(config: Path, reports_dir: Path, dry: bool) -> int:
    data = tomllib.loads(config.read_text())
    cfg = data.get("sweep", data)
    target = cfg["target"]
    kernel = cfg["kernel"]
    preset = cfg.get("preset", f"{target}-host")
    points = _cartesian(cfg.get("points", {}))

    print(f"Sweep: {len(points)} point(s)  target={target}  kernel={kernel}  preset={preset}")
    any_fail = False

    for pt in points:
        bdir = _build_dir(target, pt)
        print(f"\n--- point: {pt or 'default'} ---")

        cmake_vars: list[str] = []
        if "clock" in pt:
            cmake_vars += [f"-DANVIL_CLOCK_MHZ={pt['clock']}"]

        cmds = [
            # -B overrides the preset's binaryDir — each point gets its own build dir.
            ["cmake", "--preset", preset, "-B", bdir] + cmake_vars,
            ["cmake", "--build", bdir, "--target", f"{kernel}_xo"],
            [
                "env", "PYTHONPATH=tools", sys.executable, "-m", "hlsflow", "collect",
                "--build-dir", bdir, "--kernel", kernel, "--target", "csynth",
                "--platform", target, "--reports-dir", str(reports_dir),
            ],
        ]
        for cmd in cmds:
            rc = _run(cmd, dry)
            if rc:
                print(f"  FAILED (rc={rc}): {cmd[0]}")
                any_fail = True
                break

    return 1 if any_fail else 0


def main() -> None:
    ap = argparse.ArgumentParser(description="DSE parameter sweep")
    ap.add_argument("config", type=Path, help="TOML sweep config")
    ap.add_argument("--dry-run", action="store_true", help="Print commands, don't run")
    ap.add_argument("--reports-dir", type=Path, default=Path("reports"))
    args = ap.parse_args()
    sys.exit(sweep(args.config, args.reports_dir, args.dry_run))


if __name__ == "__main__":
    main()
