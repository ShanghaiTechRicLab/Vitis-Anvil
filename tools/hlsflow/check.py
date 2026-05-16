"""Threshold checks against stored RunRecord metrics."""
from __future__ import annotations

from dataclasses import dataclass

from hlsflow.database import RunRecord


@dataclass
class CheckResult:
    name: str
    actual: float | int | None
    limit: float | int
    op: str
    passed: bool

    def format(self) -> str:
        actual = "?" if self.actual is None else self.actual
        mark = "PASS" if self.passed else "FAIL"
        return f"{mark}: {self.name} actual={actual} {self.op} limit={self.limit}"


def run_checks(record: RunRecord, *, max_ii: int | None = None,
               max_lut: int | None = None, max_ff: int | None = None,
               max_dsp: int | None = None, max_bram: int | None = None,
               max_uram: int | None = None, max_latency: int | None = None,
               min_latency: int | None = None) -> list[CheckResult]:
    m = record.metrics
    results: list[CheckResult] = []

    def add_max(name: str, key: str, lim: int | None) -> None:
        if lim is None:
            return
        v = m.get(key)
        results.append(CheckResult(name, v, lim, "<=", v is not None and v <= lim))

    add_max("II", "worst_loop_ii", max_ii)
    add_max("LUT", "lut", max_lut)
    add_max("FF", "ff", max_ff)
    add_max("DSP", "dsp", max_dsp)
    add_max("BRAM_18K", "bram_18k", max_bram)
    add_max("URAM", "uram", max_uram)
    add_max("latency_max", "latency_max", max_latency)
    if min_latency is not None:
        v = m.get("latency_min")
        results.append(CheckResult("latency_min", v, min_latency, ">=", v is not None and v >= min_latency))
    return results
