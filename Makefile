# Vitis-Anvil top-level Makefile
# Usage: make [target] [TARGET=u250|zcu104] [LANG=cpp|python] [DATASET=tiny]

TARGET  ?= u250
LANG    ?= cpp
DATASET ?= tiny

include config/$(TARGET)/anvil.mk

BUILD_DIR   := build/$(ANVIL_PRESET)
HOST_BIN    := $(BUILD_DIR)/src/host/run_saxpy
XCLBIN_PATH := $(BUILD_DIR)/src/kernels/saxpy_xclbin/saxpy.xclbin
PYTHON      := .venv/bin/python
PIP         := $(PYTHON) -m pip

.PHONY: all configure build build-cpp build-python clean clean-all help \
        csynth cosim xclbin xclbin-hwemu \
        gen gold run-host xrt-emu xrt-hw compare analyze \
        test test-csynth test-cosim test-xrt-emu test-slow test-all

all: build

configure:
	rtk cmake --preset $(ANVIL_PRESET)

build: configure build-cpp build-python

build-cpp:
	rtk cmake --build --preset $(ANVIL_PRESET)

build-python:
	rtk rm -rf .venv
	rtk python3 -m venv .venv
	rtk $(PIP) install -e ".[test]" --quiet

csynth: configure
	rtk cmake --build $(BUILD_DIR) --target saxpy_xo

cosim: configure
	rtk cmake --build $(BUILD_DIR) --target saxpy_cosim

xclbin: configure
	rtk cmake --build $(BUILD_DIR) --target saxpy_xclbin

xclbin-hwemu: configure
	rtk cmake --build $(BUILD_DIR) --target saxpy_xclbin

gen:
	@rtk test -f scripts/gen_dataset.py || { rtk echo "scripts/gen_dataset.py is added in Task 15" >&2; exit 1; }
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/gen_dataset.py --dataset $(DATASET)

gold: build gen
	@rtk test -f scripts/run_gold.sh || { rtk echo "scripts/run_gold.sh is added in Task 15" >&2; exit 1; }
	rtk env LANG=$(LANG) DATASET=$(DATASET) ANVIL_PRESET=$(ANVIL_PRESET) bash scripts/run_gold.sh

run-host: build
	@rtk test -x $(HOST_BIN) || { rtk echo "$(HOST_BIN) not built; use a preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; }
	@rtk test -f $(XCLBIN_PATH) || { rtk echo "$(XCLBIN_PATH) not found; run make xclbin first" >&2; exit 1; }
	rtk $(HOST_BIN) --xclbin $(XCLBIN_PATH)

xrt-emu: build
	rtk env XCL_EMULATION_MODE=hw_emu $(MAKE) run-host

xrt-hw: build
	rtk $(MAKE) run-host

compare:
	@rtk test -f scripts/compare.py || { rtk echo "scripts/compare.py is added in Task 16" >&2; exit 1; }
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/compare.py --dataset $(DATASET)

analyze:
	@rtk test -f scripts/analyze.py || { rtk echo "scripts/analyze.py is added in Task 16" >&2; exit 1; }
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/analyze.py --build-dir $(BUILD_DIR)

# make test uses hls-model-linux-debug preset (CPU-only, no Vitis/XRT/xpfm).
# TARGET variable is intentionally ignored here.
test:
	rtk cmake --preset hls-model-linux-debug
	rtk cmake --build --preset hls-model-linux-debug
	rtk ctest --preset hls-model-linux-debug
	rtk $(MAKE) build-python
	rtk $(PYTHON) -m pytest -m fast tests/python -v

# Label-specific targets bypass preset filter via --test-dir.
test-csynth: configure
	rtk ctest --test-dir $(BUILD_DIR) -L csynth -V

test-cosim: configure
	rtk ctest --test-dir $(BUILD_DIR) -L cosim -V

test-xrt-emu: configure
	rtk ctest --test-dir $(BUILD_DIR) -L xrt_emu -V

test-slow: configure
	rtk ctest --test-dir $(BUILD_DIR) -L "csynth|cosim|xrt_emu" -V
	rtk $(PYTHON) -m pytest -m slow tests/python -v

test-all: test test-slow

clean:
	rtk rm -rf $(BUILD_DIR)

clean-all:
	rtk rm -rf build/ *.egg-info python/anvil.egg-info

help:
	@rtk echo "Targets:"
	@rtk echo "  make build [TARGET=u250|zcu104]  — configure + build C++ + pip install"
	@rtk echo "  make test                         — CPU-only fast tests (no Vitis/XRT)"
	@rtk echo "  make csynth                       — v++ HLS synthesis"
	@rtk echo "  make cosim                        — HLS co-simulation"
	@rtk echo "  make xclbin                       — link .xclbin"
	@rtk echo "  make gen                          — generate dataset"
	@rtk echo "  make gold [LANG=cpp|python]       — run gold reference"
	@rtk echo "  make xrt-emu                      — run on hw_emu"
	@rtk echo "  make xrt-hw                       — run on real hardware"
	@rtk echo "  make compare                      — compare outputs vs gold"
	@rtk echo "  make analyze                      — parse HLS csynth report"
	@rtk echo "  make test-csynth/cosim/xrt-emu    — label-specific hardware tests"
	@rtk echo "  make clean [TARGET=...]           — remove preset build dir"
	@rtk echo "  make clean-all                    — remove all build dirs + eggs"
