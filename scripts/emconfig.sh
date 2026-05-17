#!/usr/bin/env bash
# emconfig.sh — generate target-level emconfig.json using emconfigutil.
set -euo pipefail

PLATFORM="${ANVIL_PLATFORM:-}"
BUILD_DIR="${BUILD_DIR:-${EMCONFIG_DIR:-build/${TARGET:-unknown}/emconfig}}"

if [[ -z "${PLATFORM}" ]]; then
    echo "ERROR: ANVIL_PLATFORM not set. Source config/<target>/anvil.mk or export it." >&2
    exit 1
fi

if ! command -v emconfigutil >/dev/null 2>&1; then
    echo "ERROR: emconfigutil not in PATH. Source Vitis settings64.sh." >&2
    exit 1
fi

mkdir -p "${BUILD_DIR}"
emconfigutil --platform "${PLATFORM}" --od "${BUILD_DIR}"
echo "[emconfig] wrote ${BUILD_DIR}/emconfig.json"
