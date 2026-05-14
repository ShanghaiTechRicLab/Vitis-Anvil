"""anvil.progress — tqdm wrapper mirroring anvil::progress C++ API."""
from __future__ import annotations

from collections.abc import Iterable
from typing import TypeVar

from tqdm import tqdm

T = TypeVar("T")


class Bar:
    def __init__(self, desc: str, total: int) -> None:
        self._bar = tqdm(total=total, desc=desc, unit="it")

    def tick(self) -> None:
        self._bar.update(1)

    def done(self) -> None:
        self._bar.close()

    def __enter__(self) -> "Bar":
        return self

    def __exit__(self, *_: object) -> None:
        self.done()


def wrap(iterable: Iterable[T], desc: str = ""):
    return tqdm(iterable, desc=desc)
