"""anvil.gold — gold reference I/O helpers mirroring anvil::gold C++ API."""
from __future__ import annotations

from pathlib import Path
import os
from collections.abc import Callable
from typing import TypeAlias

import numpy as np


def load_vector(path: Path | str, dtype: np.dtype) -> np.ndarray:
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"anvil.gold: file not found: {path}")
    itemsize = np.dtype(dtype).itemsize
    size = path.stat().st_size
    if size % itemsize != 0:
        raise ValueError(f"anvil.gold: file size {size} is not divisible by dtype size {itemsize}: {path}")
    return np.fromfile(path, dtype=dtype)


def dump_vector(path: Path | str, data: np.ndarray) -> None:
    path = Path(path)
    arr = np.asarray(data)
    with path.open("wb") as f:
        written = f.write(arr.tobytes(order="C"))
        if written != arr.nbytes:
            raise OSError(f"anvil.gold: short write to {path}: {written} of {arr.nbytes} bytes")
        f.flush()
        os.fsync(f.fileno())


GoldFn: TypeAlias = Callable[[np.ndarray, np.ndarray], None]
