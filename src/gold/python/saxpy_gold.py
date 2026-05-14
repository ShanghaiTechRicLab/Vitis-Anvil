#!/usr/bin/env python3
"""Python saxpy gold reference. User-replaceable demo."""
from __future__ import annotations

import json
from pathlib import Path

import click
import numpy as np

import anvil.gold as gold
import anvil.log as log


def saxpy_gold(x: np.ndarray, y: np.ndarray, a: float) -> np.ndarray:
    """Compute ``a * x + y`` as the reference implementation."""
    return a * x + y


@click.command()
@click.option("--data-dir", required=True, type=click.Path(path_type=Path), help="Dataset dir with x.bin, y.bin, meta.json")
@click.option("--output-dir", default=None, type=click.Path(path_type=Path), help="Output dir (defaults to data-dir)")
def main(data_dir: Path, output_dir: Path | None) -> None:
    log.init("saxpy_gold_python")
    out_dir = output_dir if output_dir is not None else data_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    meta = json.loads((data_dir / "meta.json").read_text(encoding="utf-8"))
    a = float(meta["a"])
    n = int(meta["n"])

    log.info("loading x.bin/y.bin (n={} a={})", n, a)
    x = gold.load_vector(data_dir / meta.get("x", "x.bin"), dtype=np.float32)
    y = gold.load_vector(data_dir / meta.get("y", "y.bin"), dtype=np.float32)
    if x.size != n or y.size != n:
        raise click.ClickException(f"input size mismatch: n={n} x={x.size} y={y.size}")

    out = saxpy_gold(x, y, a)
    out_path = out_dir / "gold_out.bin"
    gold.dump_vector(out_path, out.astype(np.float32, copy=False))
    log.info("wrote {}", out_path)


if __name__ == "__main__":
    main()
