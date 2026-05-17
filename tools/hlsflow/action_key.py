"""Stage-specific action-key policies for HLS/Vitis build steps.

The module provides deterministic canonical JSON hashing plus lightweight policy
objects that choose which action inputs are allowed to influence each build
stage.  It is intentionally standalone so future orchestration scripts can use
it without importing the CLI.
"""

from __future__ import annotations

import dataclasses
import enum
import hashlib
import json
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, ClassVar


ACTION_KEY_SCHEMA_VERSION = 1


class ActionKeyError(ValueError):
    """Raised when an action-key input violates a stage policy."""


_STAGE_NAMES = (
    "data",
    "gold",
    "hls-model",
    "host",
    "csynth",
    "cosim",
    "xclbin",
    "emconfig",
    "run",
    "analyze",
    "deploy",
)

_MODE_FIELDS = frozenset(
    {
        "mode_affects_output",
        "mode_compile_flags",
        "mode_cflags",
        "mode_cxxflags",
        "mode_ldflags",
        "mode_config",
        "mode_configs",
        "mode_hls_config",
        "mode_vitis_config",
        "per_mode_compile_flags",
        "per_mode_config",
        "target_mode_compile_flags",
        "target_mode_config",
    }
)

_HLS_MODEL_FORBIDDEN = frozenset(
    {
        "platform",
        "platform_name",
        "platform_path",
        "xpfm",
        "vitis",
        "vitis_version",
        "vitis_hls_version",
        "vitis_settings",
        "xrt",
        "xrt_version",
        "xrt_path",
    }
)

_DEFAULT_IGNORED = frozenset(
    {
        # Caller metadata that should not change the semantic action identity.
        "key",
        "action_key",
        "cache_key",
        "output",
        "outputs",
        "output_dir",
        "build_dir",
        "timestamp",
        "created_at",
        "updated_at",
    }
)


@dataclass(frozen=True)
class StageKeyPolicy:
    """Filtering policy for one action-key stage.

    Policies remove operational metadata, reject forbidden contamination in strict
    mode, then apply stage-specific target-mode handling before hashing.
    Unknown non-forbidden input keys are retained so new scripts can add fields
    without changing this module every time a stage gains an input.
    """

    stage: str
    ignored_fields: frozenset[str] = field(default_factory=lambda: _DEFAULT_IGNORED)
    forbidden_fields: frozenset[str] = field(default_factory=frozenset)
    include_target_mode: bool = False

    _POLICIES: ClassVar[dict[str, "StageKeyPolicy"] | None] = None

    def __post_init__(self) -> None:
        if self.stage not in _STAGE_NAMES:
            raise ActionKeyError(f"unknown action-key stage: {self.stage}")

    @classmethod
    def for_stage(cls, stage: str) -> "StageKeyPolicy":
        """Return the registered policy for *stage*."""

        policies = cls._registered()
        try:
            return policies[stage]
        except KeyError as exc:
            raise ActionKeyError(f"unknown action-key stage: {stage}") from exc

    @classmethod
    def stage_names(cls) -> tuple[str, ...]:
        """Return all registered stage names in policy order."""

        return _STAGE_NAMES

    @classmethod
    def _registered(cls) -> dict[str, "StageKeyPolicy"]:
        if cls._POLICIES is None:
            cls._POLICIES = {
                "data": StageKeyPolicy("data"),
                "gold": StageKeyPolicy("gold"),
                "hls-model": StageKeyPolicy(
                    "hls-model",
                    ignored_fields=_DEFAULT_IGNORED | frozenset({"target_mode"}),
                    forbidden_fields=_HLS_MODEL_FORBIDDEN,
                ),
                "host": StageKeyPolicy("host"),
                "csynth": StageKeyPolicy("csynth"),
                "cosim": StageKeyPolicy("cosim"),
                "xclbin": StageKeyPolicy("xclbin", include_target_mode=True),
                "emconfig": StageKeyPolicy("emconfig"),
                "run": StageKeyPolicy("run", include_target_mode=True),
                "analyze": StageKeyPolicy("analyze"),
                "deploy": StageKeyPolicy("deploy"),
            }
        return cls._POLICIES

    def filtered_inputs(self, inputs: Mapping[str, Any], *, strict: bool = True) -> dict[str, Any]:
        """Return the policy-filtered, normalized input mapping.

        ``strict=True`` raises on forbidden fields.  ``strict=False`` omits those
        fields, which is useful when callers sanitize broad environment records.
        """

        forbidden_present = sorted(k for k in inputs if k in self.forbidden_fields)
        if forbidden_present and strict:
            joined = ", ".join(forbidden_present)
            raise ActionKeyError(f"{self.stage} action key forbids input field(s): {joined}")

        ignored = set(self.ignored_fields)
        ignored.update(self.forbidden_fields)
        ignored.add("stage")

        result = {k: v for k, v in inputs.items() if k not in ignored}
        self._apply_target_mode_policy(result, inputs)
        return result

    def _apply_target_mode_policy(self, result: dict[str, Any], original: Mapping[str, Any]) -> None:
        if self.include_target_mode:
            return

        if self.stage == "host" and original.get("device_kind") == "embedded":
            return

        if self.stage in {"csynth", "cosim"} and _mode_can_affect_output(original):
            return

        result.pop("target_mode", None)

    def payload(self, inputs: Mapping[str, Any], *, strict: bool = True) -> dict[str, Any]:
        """Build the complete canonical-hash payload for this stage."""

        return {
            "schema": ACTION_KEY_SCHEMA_VERSION,
            "stage": self.stage,
            "inputs": self.filtered_inputs(inputs, strict=strict),
        }

    def key(self, inputs: Mapping[str, Any], *, strict: bool = True) -> str:
        """Return the SHA-256 action key for *inputs* under this policy."""

        return canonical_json_hash(self.payload(inputs, strict=strict))


def _mode_can_affect_output(inputs: Mapping[str, Any]) -> bool:
    if bool(inputs.get("mode_affects_output")):
        return True
    return any(name in inputs for name in _MODE_FIELDS - {"mode_affects_output"})


def canonical_json(value: Any) -> str:
    """Serialize *value* as deterministic canonical JSON.

    Object keys are sorted, insignificant whitespace is removed, and NaN/Infinity
    are rejected by ``json.dumps(..., allow_nan=False)``.
    """

    return json.dumps(
        _normalize(value),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    )


def canonical_json_hash(value: Any) -> str:
    """Return the SHA-256 hex digest of ``canonical_json(value)``."""

    return hashlib.sha256(canonical_json(value).encode("utf-8")).hexdigest()


def action_key(stage: str, inputs: Mapping[str, Any], *, strict: bool = True) -> str:
    """Return the stage-specific action key for *inputs*."""

    return StageKeyPolicy.for_stage(stage).key(inputs, strict=strict)


def action_key_payload(stage: str, inputs: Mapping[str, Any], *, strict: bool = True) -> dict[str, Any]:
    """Return the canonical payload that would be hashed for *stage*."""

    return StageKeyPolicy.for_stage(stage).payload(inputs, strict=strict)


def _normalize(value: Any) -> Any:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return _normalize(dataclasses.asdict(value))

    if isinstance(value, enum.Enum):
        return _normalize(value.value)

    if isinstance(value, Path):
        return str(value)

    if isinstance(value, Mapping):
        normalized: dict[str, Any] = {}
        for key, item in value.items():
            if not isinstance(key, str):
                key = str(key)
            normalized[key] = _normalize(item)
        return normalized

    if isinstance(value, (set, frozenset)):
        items = [_normalize(item) for item in value]
        return sorted(items, key=lambda item: json.dumps(item, sort_keys=True, separators=(",", ":")))

    if isinstance(value, Sequence) and not isinstance(value, (str, bytes, bytearray)):
        return [_normalize(item) for item in value]

    if isinstance(value, (bytes, bytearray)):
        return value.hex()

    return value

# Convenience API used by action-runner scripts.  This wraps the
# policy registry above without changing the canonical StageKeyPolicy behavior.
class Stage(str, enum.Enum):
    DATA = "data"
    GOLD = "gold"
    HLS_MODEL = "hls-model"
    HOST = "host"
    CSYNTH = "csynth"
    COSIM = "cosim"
    XCLBIN = "xclbin"
    EMCONFIG = "emconfig"
    RUN = "run"
    ANALYZE = "analyze"
    DEPLOY = "deploy"


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def file_digest(path: str | Path) -> str:
    h = hashlib.sha256()
    with Path(path).open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def declared_input(path: str | Path, *, digest: str | None = None) -> dict[str, str]:
    p = Path(path)
    return {"path": str(p), "sha256": digest if digest is not None else file_digest(p)}


def env_key(env: Mapping[str, Any]) -> str:
    return canonical_json_hash(dict(env))


def write_json_if_changed(path: str | Path, data: Mapping[str, Any]) -> None:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    content = json.dumps(_normalize(data), sort_keys=True, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    if p.exists() and p.read_text(encoding="utf-8") == content:
        return
    p.write_text(content, encoding="utf-8")


@dataclass(frozen=True)
class ActionKeySpec:
    stage: Stage | str
    target_identity: str | None = None
    mode_identity: str | None = None
    target_kind: str | None = None
    toolchain: Mapping[str, Any] = field(default_factory=dict)
    platform: Mapping[str, Any] = field(default_factory=dict)
    command_args: Sequence[str] = field(default_factory=tuple)
    env_whitelist: Mapping[str, str] = field(default_factory=dict)
    declared_inputs: Sequence[Mapping[str, Any]] = field(default_factory=tuple)
    output_names: Sequence[str] = field(default_factory=tuple)
    config_digest: str | None = None
    mode_affects_output: bool = False
    extra: Mapping[str, Any] = field(default_factory=dict)

    def _inputs(self) -> dict[str, Any]:
        stage = self.stage.value if isinstance(self.stage, Stage) else str(self.stage)
        if stage == Stage.HLS_MODEL.value:
            data: dict[str, Any] = {
                "command_args": list(self.command_args),
                "declared_inputs": list(self.declared_inputs),
                "output_names": list(self.output_names),
                "config_digest": self.config_digest,
                **dict(self.extra),
            }
        else:
            data = {
                "target": self.target_identity,
                "target_mode": self.mode_identity,
                "device_kind": self.target_kind,
                "toolchain": dict(self.toolchain),
                "platform": dict(self.platform),
                "command_args": list(self.command_args),
                "env_whitelist": dict(self.env_whitelist),
                "declared_inputs": list(self.declared_inputs),
                "output_names": list(self.output_names),
                "config_digest": self.config_digest,
                "mode_affects_output": self.mode_affects_output,
                **dict(self.extra),
            }
        if stage in {Stage.CSYNTH.value, Stage.COSIM.value} and self.mode_affects_output:
            data["mode_config"] = self.mode_identity
        return {k: v for k, v in data.items() if v is not None}

    def policy_dict(self) -> dict[str, Any]:
        stage = self.stage.value if isinstance(self.stage, Stage) else str(self.stage)
        return action_key_payload(stage, self._inputs(), strict=False)

    def policy_json(self) -> str:
        return canonical_json(self.policy_dict())

    def key(self) -> str:
        return canonical_json_hash(self.policy_dict())
