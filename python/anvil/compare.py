"""anvil.compare — numpy-backed compare functions mirroring anvil::compare C++ API."""
from __future__ import annotations

from typing import TypeAlias

import numpy as np

ArrayLike: TypeAlias = np.ndarray | list[float] | list[int]


def _to_arrays(a: ArrayLike, b: ArrayLike) -> tuple[np.ndarray, np.ndarray]:
    arr_a = np.asarray(a)
    arr_b = np.asarray(b)
    if arr_a.shape != arr_b.shape:
        raise ValueError(f"shape mismatch: {arr_a.shape} vs {arr_b.shape}")
    return arr_a.astype(np.float64), arr_b.astype(np.float64)


def max_abs_error(a: ArrayLike, b: ArrayLike) -> float:
    arr_a, arr_b = _to_arrays(a, b)
    if not arr_a.size:
        return 0.0
    # Match C++ std::max loop behavior: NaN diffs do not replace the current max.
    diffs = np.abs(arr_a - arr_b)
    finite_diffs = diffs[~np.isnan(diffs)]
    return float(np.max(finite_diffs)) if finite_diffs.size else 0.0


def rms_error(a: ArrayLike, b: ArrayLike) -> float:
    arr_a, arr_b = _to_arrays(a, b)
    return float(np.sqrt(np.mean((arr_a - arr_b) ** 2))) if arr_a.size else 0.0


def mean_abs_error(a: ArrayLike, b: ArrayLike) -> float:
    arr_a, arr_b = _to_arrays(a, b)
    return float(np.mean(np.abs(arr_a - arr_b))) if arr_a.size else 0.0


def bit_exact(a: ArrayLike, b: ArrayLike) -> bool:
    arr_a = np.ascontiguousarray(np.asarray(a))
    arr_b = np.ascontiguousarray(np.asarray(b))
    if arr_a.shape != arr_b.shape or arr_a.dtype != arr_b.dtype:
        return False
    return bool(np.array_equal(arr_a.view(np.uint8), arr_b.view(np.uint8)))


def psnr(reference: ArrayLike, test: ArrayLike, peak: float) -> float:
    rms = rms_error(reference, test)
    if rms == 0.0:
        return float("inf")
    return float(20.0 * np.log10(float(peak) / rms))


def snr(reference: ArrayLike, test: ArrayLike) -> float:
    ref, tst = _to_arrays(reference, test)
    sig_power = float(np.sum(ref**2))
    noise_power = float(np.sum((ref - tst) ** 2))
    if noise_power == 0.0:
        return float("inf")
    if sig_power == 0.0:
        return float("-inf")
    return float(10.0 * np.log10(sig_power / noise_power))


def cosine_similarity(a: ArrayLike, b: ArrayLike) -> float:
    arr_a, arr_b = _to_arrays(a, b)
    denom = np.linalg.norm(arr_a) * np.linalg.norm(arr_b)
    return 0.0 if denom == 0.0 else float(np.dot(arr_a.ravel(), arr_b.ravel()) / denom)
