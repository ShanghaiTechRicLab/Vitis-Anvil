#!/usr/bin/env python3
"""Generate saxpy input dataset: x.bin, y.bin, meta.json."""
from __future__ import annotations

import json
from pathlib import Path

import click
import numpy as np

import anvil.log as log
import anvil.progress as progress


@click.command()
@click.option("--dataset", default="tiny", help="Dataset name (subdir of data/)")
@click.option("--n", default=1024, type=int, help="Number of elements")
@click.option("--a", default=2.0, type=float, help="Scalar coefficient")
@click.option("--seed", default=42, type=int, help="RNG seed")
def main(dataset: str, n: int, a: float, seed: int) -> None:
    log.init("gen_dataset")
    if n <= 0:
        raise click.BadParameter("--n must be positive")
    if not dataset or "/" in dataset or "\\" in dataset or dataset in {".", ".."}:
        raise click.BadParameter("--dataset must be a simple name without path separators")

    out_dir = Path("data") / dataset
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(seed)
    x = rng.random(n, dtype=np.float32)
    y = rng.random(n, dtype=np.float32)

    log.info("generating dataset {!r}: n={} a={} seed={}", dataset, n, a, seed)
    bar = progress.Bar("writing", 3)
    x.tofile(out_dir / "x.bin")
    bar.tick()
    y.tofile(out_dir / "y.bin")
    bar.tick()
    meta = {"case_id": dataset, "dataset": dataset, "n": n, "a": a, "seed": seed, "x": "x.bin", "y": "y.bin", "dtype": "float32"}
    (out_dir / "meta.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    bar.tick()
    bar.done()
    log.info("wrote {}/x.bin, y.bin, meta.json", out_dir)


if __name__ == "__main__":
    main()
