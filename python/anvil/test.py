"""anvil.test — pytest helpers mirroring anvil::test C++ macros."""
from __future__ import annotations

from collections.abc import Sequence
from typing import Callable, TypeVar

import pytest

F = TypeVar("F", bound=Callable[..., object])


def parametric_sizes(sizes: Sequence[int]):
    """Decorator: run test with each n in sizes."""
    return pytest.mark.parametrize("n", list(sizes))


def approx_case(tol: float):
    """Decorator factory: inject a single `tol` parameter into a pytest case."""
    return pytest.mark.parametrize("tol", [tol])
