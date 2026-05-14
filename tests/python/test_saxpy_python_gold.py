import numpy as np
import pytest

from src.gold.python.saxpy_gold import saxpy_gold

pytestmark = pytest.mark.fast


def test_saxpy_python_gold_basic():
    x = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    y = np.array([10.0, 20.0, 30.0], dtype=np.float32)
    out = saxpy_gold(x, y, a=2.0)
    expected = np.array([12.0, 24.0, 36.0], dtype=np.float32)
    np.testing.assert_array_almost_equal(out, expected)


def test_saxpy_python_gold_n1():
    x = np.array([3.0], dtype=np.float32)
    y = np.array([4.0], dtype=np.float32)
    out = saxpy_gold(x, y, a=2.0)
    assert out[0] == pytest.approx(10.0)
