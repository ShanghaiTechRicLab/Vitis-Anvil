from pathlib import Path
import tempfile

import numpy as np
import pytest
from anvil import gold

pytestmark = pytest.mark.fast


def test_dump_and_load_roundtrip():
    data = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "test.bin"
        gold.dump_vector(path, data)
        loaded = gold.load_vector(path, dtype=np.float32)
        np.testing.assert_array_equal(data, loaded)


def test_load_nonexistent_raises():
    with pytest.raises(FileNotFoundError):
        gold.load_vector("/nonexistent/path.bin", dtype=np.float32)


def test_load_malformed_size_raises():
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "bad.bin"
        path.write_bytes(b"abc")
        with pytest.raises(ValueError):
            gold.load_vector(path, dtype=np.float32)


def test_dump_vector_reports_write_failure():
    if not Path("/dev/full").exists():
        pytest.skip("/dev/full unavailable on this platform")
    with pytest.raises(OSError):
        gold.dump_vector("/dev/full", np.array([1.0], dtype=np.float32))


def test_dump_missing_parent_raises():
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "missing" / "out.bin"
        with pytest.raises(OSError):
            gold.dump_vector(path, np.array([1.0], dtype=np.float32))
