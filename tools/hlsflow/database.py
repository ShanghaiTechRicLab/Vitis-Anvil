"""Append-only JSONL store for hlsflow run records."""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
import json
from pathlib import Path
from typing import Any
import uuid


@dataclass
class RunRecord:
    run_id: str
    kernel: str
    platform: str
    target: str
    git_commit: str
    build_dir: str
    vitis_version: str
    status: str
    timestamp: str
    reports: dict[str, str] = field(default_factory=dict)
    metrics: dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> str:
        return json.dumps(asdict(self), separators=(",", ":"))


def now_run_id(kernel: str, platform: str) -> str:
    ts = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    return f"{kernel}_{platform}_{ts}_{uuid.uuid4().hex[:6]}"


def append(record: RunRecord, store: Path) -> None:
    store.parent.mkdir(parents=True, exist_ok=True)
    with store.open("a", encoding="utf-8") as f:
        f.write(record.to_json() + "\n")


def load_all(store: Path) -> list[RunRecord]:
    if not store.exists():
        return []
    records: list[RunRecord] = []
    with store.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                records.append(RunRecord(**json.loads(line)))
    return records


def find_by_id(run_id: str, store: Path) -> RunRecord | None:
    for record in load_all(store):
        if record.run_id == run_id:
            return record
    return None


def latest(store: Path, kernel: str | None = None, target: str | None = None) -> RunRecord | None:
    records = load_all(store)
    if kernel:
        records = [r for r in records if r.kernel == kernel]
    if target:
        records = [r for r in records if r.target == target]
    return records[-1] if records else None
