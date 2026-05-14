#!/usr/bin/env python3
"""Compare gold vs HLS model vs XRT outputs for a dataset."""
from __future__ import annotations

from pathlib import Path
import sys

import click
import numpy as np

import anvil.compare as cmp
import anvil.log as log
import anvil.table as tbl


def _validate_dataset(_ctx: click.Context, _param: click.Parameter, value: str) -> str:
    if not value or "/" in value or "\\" in value or value in {".", ".."}:
        raise click.BadParameter("dataset must be a simple name without path separators")
    return value


def load_bin(path: Path) -> np.ndarray | None:
    if path.exists():
        return np.fromfile(path, dtype=np.float32)
    return None


@click.command()
@click.option("--dataset", default="tiny", callback=_validate_dataset, help="Dataset name")
@click.option("--tol", default=1e-5, type=float, help="Max-abs-error tolerance for PASS")
def main(dataset: str, tol: float) -> None:
    log.init("compare")
    data_dir = Path("data") / dataset

    meta_path = data_dir / "meta.json"
    if not meta_path.exists():
        log.error("meta.json not found in {}. Run 'make gen' first.", data_dir)
        sys.exit(1)

    gold = load_bin(data_dir / "gold_out.bin")
    if gold is None:
        gold = load_bin(data_dir / f"{dataset}_gold_out.bin")
    hls = load_bin(data_dir / "hls_model_out.bin")
    xrt_emu = load_bin(data_dir / "xrt_emu_out.bin")
    xrt_hw = load_bin(data_dir / "xrt_hw_out.bin")

    rows = [["impl", "max_abs", "rms", "verdict"]]
    compared = False
    any_fail = False
    if gold is not None:
        rows.append(["gold", "0", "0", "ref"])
    for label, data in [("hls_model", hls), ("xrt_emu", xrt_emu), ("xrt_hw", xrt_hw)]:
        if data is None:
            continue
        if gold is None:
            log.error("{} exists but no gold output was found for dataset {}", label, dataset)
            sys.exit(1)
        try:
            mae = cmp.max_abs_error(gold, data)
            rms = cmp.rms_error(gold, data)
        except ValueError as exc:
            log.error("{} compare failed: {}", label, exc)
            sys.exit(1)
        verdict = "PASS" if mae <= tol else "FAIL"
        compared = True
        any_fail = any_fail or verdict == "FAIL"
        rows.append([label, f"{mae:.3e}", f"{rms:.3e}", verdict])

    tbl.print_table(rows)
    if not compared:
        log.warn("no implementation outputs found for dataset {}; nothing to compare", dataset)
        sys.exit(1)
    sys.exit(1 if any_fail else 0)


if __name__ == "__main__":
    main()
