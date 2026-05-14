#!/usr/bin/env bash
# run_gold.sh — dispatch C++ or Python gold for data/<dataset>.
set -euo pipefail

ANVIL_LANG="${ANVIL_LANG:-cpp}"
DATASET="${DATASET:-tiny}"
PRESET="${ANVIL_PRESET:-hls-model-linux-debug}"
BUILD_DIR="build/${PRESET}"

if [[ -z "${DATASET}" || "${DATASET}" == "." || "${DATASET}" == ".." || "${DATASET}" == *"/"* || "${DATASET}" == *"\\"* ]]; then
    echo "ERROR: DATASET must be a simple name without path separators." >&2
    exit 1
fi

DATA_DIR="data/${DATASET}"

if [[ ! -f "${DATA_DIR}/meta.json" ]]; then
    echo "ERROR: ${DATA_DIR}/meta.json not found. Run 'make gen' first." >&2
    exit 1
fi

case "${ANVIL_LANG}" in
cpp)
    GOLD_BIN="${BUILD_DIR}/src/apps/run_gold"
    if [[ ! -x "${GOLD_BIN}" ]]; then
        echo "ERROR: ${GOLD_BIN} not found. Run 'make build-cpp' first." >&2
        exit 1
    fi
    echo "[run_gold] running C++ gold for dataset=${DATASET}"
    "${GOLD_BIN}" --case "${DATA_DIR}/meta.json" --data-dir "${DATA_DIR}" --output-dir "${DATA_DIR}"
    ;;
python)
    if [[ ! -f src/gold/python/saxpy_gold.py ]]; then
        echo "ERROR: src/gold/python/saxpy_gold.py is added in Task 17." >&2
        exit 1
    fi
    echo "[run_gold] running Python gold for dataset=${DATASET}"
    .venv/bin/python src/gold/python/saxpy_gold.py --data-dir "${DATA_DIR}" --output-dir "${DATA_DIR}"
    ;;
*)
    echo "ERROR: unknown ANVIL_LANG=${ANVIL_LANG}. Use ANVIL_LANG=cpp or ANVIL_LANG=python." >&2
    exit 1
    ;;
esac
echo "[run_gold] done"
