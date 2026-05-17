"""Run one build action, emit action/env hashes and artifact manifest."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Any

from .action_key import ActionKeySpec, Stage, declared_input, env_key, write_json_if_changed
from .hls_deps import generate_depfile


def _read_existing_inputs(paths: list[str]) -> list[dict[str, str]]:
    result = []
    for path in paths:
        p = Path(path)
        if p.exists() and p.is_file():
            result.append(declared_input(p))
    return result


def _tool_version(command: str) -> str:
    try:
        proc = subprocess.run([command, "--version"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
        return proc.stdout.splitlines()[0] if proc.stdout else ""
    except Exception:
        return ""


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", required=True, choices=[s.value for s in Stage])
    ap.add_argument("--stamp", required=True)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--action-json", required=True)
    ap.add_argument("--action-sha", required=True)
    ap.add_argument("--env-json", required=True)
    ap.add_argument("--env-sha", required=True)
    ap.add_argument("--target-identity")
    ap.add_argument("--mode-identity")
    ap.add_argument("--target-kind")
    ap.add_argument("--platform")
    ap.add_argument("--part")
    ap.add_argument("--clock")
    ap.add_argument("--top")
    ap.add_argument("--config")
    ap.add_argument("--input", action="append", default=[])
    ap.add_argument("--output", action="append", default=[])
    ap.add_argument("--depfile")
    ap.add_argument("--dep-compiler")
    ap.add_argument("--dep-source", action="append", default=[])
    ap.add_argument("--dep-include", action="append", default=[])
    ap.add_argument("--dep-define", action="append", default=[])
    ap.add_argument("--dep-std", default="c++14")
    ap.add_argument("--mode-affects-output", action="store_true")
    ap.add_argument("command", nargs=argparse.REMAINDER)
    ns = ap.parse_args(argv)

    command = list(ns.command)
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        ap.error("command after -- is required")

    dep_inputs: list[str] = []
    if ns.depfile:
        if not ns.dep_compiler or not ns.dep_source:
            ap.error("--depfile requires --dep-compiler and --dep-source")
        dep_inputs = generate_depfile(
            compiler=ns.dep_compiler,
            sources=ns.dep_source,
            target=ns.stamp,
            output=ns.depfile,
            includes=ns.dep_include,
            defines=ns.dep_define,
            std=ns.dep_std.replace("c++", "c++"),
            extra_args=[],
        )

    inputs = _read_existing_inputs(ns.input + dep_inputs)
    config_digest = None
    if ns.config and Path(ns.config).exists():
        config_digest = declared_input(ns.config)["sha256"]
    tool = command[0]
    spec = ActionKeySpec(
        stage=ns.stage,
        target_identity=ns.target_identity,
        mode_identity=ns.mode_identity,
        target_kind=ns.target_kind,
        toolchain={"tool": tool, "version": _tool_version(tool)},
        platform={"path": ns.platform, "part": ns.part, "clock": ns.clock},
        command_args=command,
        env_whitelist={k: os.environ.get(k, "") for k in ("XILINX_VITIS", "XILINX_XRT", "XRT_ROOT")},
        declared_inputs=inputs,
        output_names=ns.output,
        config_digest=config_digest,
        mode_affects_output=ns.mode_affects_output,
        extra={"top": ns.top},
    )
    action_json = spec.policy_dict()
    write_json_if_changed(ns.action_json, action_json)
    Path(ns.action_sha).write_text(spec.key() + "\n", encoding="utf-8")
    env = {"command_tool": tool, "tool_version": _tool_version(tool), "cwd": os.getcwd()}
    write_json_if_changed(ns.env_json, env)
    Path(ns.env_sha).write_text(env_key(env) + "\n", encoding="utf-8")

    proc = subprocess.run(command)
    if proc.returncode != 0:
        return proc.returncode

    outputs: list[dict[str, Any]] = []
    for out in ns.output:
        p = Path(out)
        outputs.append({"path": out, "exists": p.exists(), "size": p.stat().st_size if p.exists() and p.is_file() else None})
    write_json_if_changed(ns.manifest, {
        "stage": ns.stage,
        "action_sha256": spec.key(),
        "outputs": outputs,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    })
    Path(ns.stamp).parent.mkdir(parents=True, exist_ok=True)
    Path(ns.stamp).write_text(datetime.now(timezone.utc).isoformat() + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
