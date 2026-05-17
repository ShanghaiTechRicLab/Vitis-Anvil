"""Compare gold_out.bin vs a hardware/emulation run output."""
from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path

import numpy as np

from hlsflow.database import RunRecord, now_run_id


def compare_gold_hw(
    dataset_dir: Path,
    kernel: str,
    platform: str,
    *,
    tol: float = 1e-5,
    git_commit: str = "unknown",
    build_dir: str = "",
    vitis_version: str = "unknown",
    hw_output: Path | None = None,
) -> RunRecord:
    """Load gold_out.bin and a run output, compute mae/rms, return RunRecord."""
    gold_path = dataset_dir / "gold_out.bin"
    if not gold_path.exists():
        legacy_gold_path = dataset_dir / f"{dataset_dir.name}_gold_out.bin"
        if legacy_gold_path.exists():
            gold_path = legacy_gold_path
    if hw_output is None:
        raise FileNotFoundError("run output path must be provided")
    hw_path = hw_output
    if not gold_path.exists():
        raise FileNotFoundError(f"gold_out.bin not found in {dataset_dir}")
    if not hw_path.exists():
        raise FileNotFoundError(f"hardware output not found: {hw_path}")

    gold = np.fromfile(gold_path, dtype=np.float32)
    hw = np.fromfile(hw_path, dtype=np.float32)
    if gold.shape != hw.shape:
        raise ValueError(f"Shape mismatch: gold={gold.shape} hw={hw.shape}")
    if gold.size == 0:
        raise ValueError(f"Empty output: no float32 elements in {gold_path}")

    diff = gold.astype(np.float64) - hw.astype(np.float64)
    mae = float(np.max(np.abs(diff)))
    rms = float(np.sqrt(np.mean(diff ** 2)))
    verdict = "pass" if mae <= tol else "fail"

    return RunRecord(
        run_id=now_run_id(kernel, platform),
        kernel=kernel,
        platform=platform,
        target="hw",
        git_commit=git_commit,
        build_dir=build_dir,
        vitis_version=vitis_version,
        status=verdict,
        timestamp=datetime.now(timezone.utc).isoformat(),
        reports={"gold_bin": str(gold_path), "run_output_bin": str(hw_path)},
        metrics={"mae": mae, "rms": rms, "tol": tol, "n_elems": int(gold.size)},
    )
