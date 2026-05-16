"""Locate Vitis HLS report files inside a CMake build tree."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

_SKIP_PATTERNS = re.compile(r"_(Pipeline|Loop)_", re.IGNORECASE)


@dataclass
class CsynthHit:
    kernel: str
    report_path: Path
    work_dir: Path


def find_csynth_reports(build_dir: Path) -> list[CsynthHit]:
    hits: list[CsynthHit] = []
    for xml in sorted(build_dir.rglob("*_csynth.xml")):
        if _SKIP_PATTERNS.search(xml.name):
            continue
        work_dir = None
        for parent in xml.parents:
            if parent.name.endswith("_hls"):
                work_dir = parent
                break
        if work_dir is None:
            continue
        kernel = work_dir.name[:-len("_hls")]
        hits.append(CsynthHit(kernel=kernel, report_path=xml, work_dir=work_dir))
    seen: dict[tuple[str, Path], CsynthHit] = {}
    for h in hits:
        seen.setdefault((h.kernel, h.work_dir), h)
    return list(seen.values())


def find_csynth_for_kernel(build_dir: Path, kernel: str) -> CsynthHit | None:
    for hit in find_csynth_reports(build_dir):
        if hit.kernel == kernel:
            return hit
    return None


def find_cosim_dir(work_dir: Path) -> Path | None:
    candidate = work_dir / "hls" / "sim" / "report"
    return candidate if candidate.is_dir() else None
