import numpy as np
import pytest
from anvil import compare

pytestmark = pytest.mark.fast


def test_max_abs_error_identical():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    assert compare.max_abs_error(a, a) == pytest.approx(0.0)


def test_max_abs_error_known():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    b = np.array([1.5, 2.0, 2.5], dtype=np.float32)
    assert compare.max_abs_error(a, b) == pytest.approx(0.5)


def test_rms_error_identical():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    assert compare.rms_error(a, a) == pytest.approx(0.0)


def test_rms_error_known():
    a = np.zeros(2, dtype=np.float32)
    b = np.ones(2, dtype=np.float32)
    assert compare.rms_error(a, b) == pytest.approx(1.0)


def test_bit_exact_identical():
    a = np.array([1.0, 2.0], dtype=np.float32)
    assert compare.bit_exact(a, a) is True


def test_bit_exact_different():
    a = np.array([1.0], dtype=np.float32)
    b = np.array([1.5], dtype=np.float32)
    assert compare.bit_exact(a, b) is False


def test_bit_exact_signed_zero_differs():
    a = np.array([0.0], dtype=np.float32)
    b = np.array([-0.0], dtype=np.float32)
    assert compare.bit_exact(a, b) is False


def test_psnr_identical_is_inf():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    assert np.isinf(compare.psnr(a, a, peak=3.0))


def test_cosine_similarity_parallel():
    a = np.array([1.0, 0.0], dtype=np.float32)
    assert compare.cosine_similarity(a, a) == pytest.approx(1.0)


def test_size_mismatch_raises():
    a = np.array([1.0, 2.0], dtype=np.float32)
    b = np.array([1.0], dtype=np.float32)
    with pytest.raises(ValueError):
        compare.max_abs_error(a, b)


def test_bit_exact_noncontiguous_slice():
    base = np.array([1.0, 9.0, 2.0, 8.0], dtype=np.float32)
    assert compare.bit_exact(base[::2], np.array([1.0, 2.0], dtype=np.float32)) is True


def test_snr_zero_signal_nonzero_noise_is_negative_inf():
    ref = np.zeros(2, dtype=np.float32)
    tst = np.ones(2, dtype=np.float32)
    assert compare.snr(ref, tst) == float("-inf")


def test_max_abs_error_ignores_nan_like_cpp_std_max_loop():
    a = np.array([1.0, np.nan], dtype=np.float32)
    b = np.array([2.0, 0.0], dtype=np.float32)
    assert compare.max_abs_error(a, b) == pytest.approx(1.0)


def test_max_abs_error_all_nan_matches_cpp_loop():
    a = np.array([np.nan], dtype=np.float32)
    b = np.array([0.0], dtype=np.float32)
    assert compare.max_abs_error(a, b) == pytest.approx(0.0)
