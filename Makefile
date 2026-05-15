# Vitis-Anvil top-level Makefile
# Usage: make [target] [TARGET=u250|zcu104|zcu102] [ANVIL_LANG=cpp|python] [DATASET=tiny]

TARGET  ?= u250
ANVIL_LANG ?= cpp
DATASET ?= tiny

include config/$(TARGET)/anvil.mk

BUILD_DIR   := build/$(ANVIL_PRESET)
ANVIL_HOST_PRESET  ?= $(ANVIL_PRESET)
HOST_BUILD_DIR     := build/$(ANVIL_HOST_PRESET)
HOST_BIN    := $(HOST_BUILD_DIR)/src/host/run_saxpy
XCLBIN_PATH := $(BUILD_DIR)/src/kernels/saxpy_xclbin/saxpy.xclbin
ANVIL_HWEMU_PRESET ?= $(ANVIL_PRESET)-hwemu
HWEMU_BUILD_DIR    := build/$(ANVIL_HWEMU_PRESET)
HWEMU_HOST_BIN     := $(HWEMU_BUILD_DIR)/src/host/run_saxpy
HWEMU_XCLBIN_PATH  := $(HWEMU_BUILD_DIR)/src/kernels/saxpy_xclbin/saxpy.xclbin
PYTHON      := .venv/bin/python
PIP         := $(PYTHON) -m pip
BOARD_IP          ?=
BOARD_SSH_USER    ?= root
BOARD_DEPLOY_DIR  ?= ~/anvil-deploy

.PHONY: all configure build build-cpp build-python clean clean-all help \
        csynth cosim xclbin xclbin-hwemu \
        gen gold run-host xrt-emu xrt-hw compare analyze emconfig \
        deploy deploy-bin deploy-xclbin deploy-data deploy-check \
        test test-csynth test-cosim test-xrt-emu test-xrt-hw test-slow test-all

all: build

configure:
	rtk cmake --preset $(ANVIL_PRESET)
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		if [ -n "$(ANVIL_SYSROOT)" ]; then \
			rtk env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET); \
		else \
			rtk cmake --preset $(ANVIL_HOST_PRESET); \
		fi; \
	fi

build: configure build-cpp build-python

build-cpp:
	rtk cmake --build --preset $(ANVIL_PRESET)
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		rtk cmake --build --preset $(ANVIL_HOST_PRESET); \
	fi

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

xclbin-hwemu:
	rtk cmake --preset $(ANVIL_HWEMU_PRESET)
	rtk cmake --build --preset $(ANVIL_HWEMU_PRESET) --target saxpy_xclbin

gen:
	@if [ ! -f scripts/gen_dataset.py ]; then rtk echo "scripts/gen_dataset.py is added in Task 15" >&2; exit 1; fi
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/gen_dataset.py --dataset $(DATASET)

gold: build gen
	@if [ ! -f scripts/run_gold.sh ]; then rtk echo "scripts/run_gold.sh is added in Task 15" >&2; exit 1; fi
	rtk env ANVIL_LANG=$(ANVIL_LANG) DATASET=$(DATASET) ANVIL_PRESET=$(ANVIL_PRESET) bash scripts/run_gold.sh

run-host: build gen
	@if [ ! -x $(HOST_BIN) ]; then rtk echo "$(HOST_BIN) not built; use a preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(XCLBIN_PATH) ]; then rtk echo "$(XCLBIN_PATH) not found; run make xclbin first" >&2; exit 1; fi
	rtk $(HOST_BIN) --xclbin $(XCLBIN_PATH) --data-dir data/$(DATASET) --output data/$(DATASET)/xrt_hw_out.bin

emconfig:
	rtk env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh

ifeq ($(ANVIL_DEVICE_KIND),embedded)
xrt-emu: gen
	rtk echo "[xrt-emu] TARGET=$(TARGET): building kernel ($(ANVIL_PRESET)) + AArch64 host ($(ANVIL_HOST_PRESET))..."
	rtk cmake --preset $(ANVIL_PRESET)
	rtk cmake --build --preset $(ANVIL_PRESET) --target saxpy_xclbin
	@if [ -n "$(ANVIL_SYSROOT)" ]; then \
		rtk env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET); \
	else \
		rtk cmake --preset $(ANVIL_HOST_PRESET); \
	fi
	rtk cmake --build --preset $(ANVIL_HOST_PRESET) --target run_saxpy
	rtk env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	rtk echo ""
	rtk echo "Embedded hw_emu requires QEMU — see docs/deploy.md#qemu-emulation"
	rtk echo "  xclbin : $(XCLBIN_PATH)"
	rtk echo "  host   : $(HOST_BIN)  (AArch64)"
	rtk echo "  emcfg  : $(HWEMU_BUILD_DIR)/emconfig.json"
else
xrt-emu: gen
	@if [ "$(ANVIL_DEVICE_KIND)" != "accelerator" ]; then rtk echo "make xrt-emu currently requires an accelerator target with a combined host+kernel hw_emu preset; got TARGET=$(TARGET) ($(ANVIL_DEVICE_KIND))" >&2; exit 1; fi
	rtk cmake --preset $(ANVIL_HWEMU_PRESET)
	rtk cmake --build --preset $(ANVIL_HWEMU_PRESET) --target run_saxpy saxpy_xclbin
	rtk env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	@if [ ! -x $(HWEMU_HOST_BIN) ]; then rtk echo "$(HWEMU_HOST_BIN) not built; use a hw_emu preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(HWEMU_XCLBIN_PATH) ]; then rtk echo "$(HWEMU_XCLBIN_PATH) not found; make xrt-emu should have built saxpy_xclbin" >&2; exit 1; fi
	rtk env XCL_EMULATION_MODE=hw_emu EMCONFIG_PATH=$(HWEMU_BUILD_DIR) $(HWEMU_HOST_BIN) --xclbin $(HWEMU_XCLBIN_PATH) --data-dir data/$(DATASET) --output data/$(DATASET)/xrt_emu_out.bin
endif

xrt-hw: build
	rtk $(MAKE) run-host

compare:
	@if [ ! -f scripts/compare.py ]; then rtk echo "scripts/compare.py is added in Task 16" >&2; exit 1; fi
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/compare.py --dataset $(DATASET)

analyze:
	@if [ ! -f scripts/analyze.py ]; then rtk echo "scripts/analyze.py is added in Task 16" >&2; exit 1; fi
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

test-xrt-emu:
	rtk $(MAKE) xrt-emu

# test-xrt-hw: build (cross-compile + xclbin) → gen → deploy+run via board_run.py → compare.
# Requires BOARD_IP and (for zcu102) PETALINUX_SYSROOT + Vitis env.
test-xrt-hw:
	@if [ -z "$(BOARD_IP)" ]; then rtk echo "ERROR: BOARD_IP not set. Usage: make test-xrt-hw BOARD_IP=<ip> [TARGET=zcu102] [DATASET=tiny]" >&2; exit 1; fi
	rtk $(MAKE) build TARGET=$(TARGET)
	rtk $(MAKE) xclbin TARGET=$(TARGET)
	rtk $(MAKE) gen DATASET=$(DATASET)
	rtk $(MAKE) build-python
	rtk $(PYTHON) scripts/board_run.py \
		--board-ip "$(BOARD_IP)" \
		--ssh-user "$(BOARD_SSH_USER)" \
		--deploy-dir "$(BOARD_DEPLOY_DIR)" \
		--xclbin "$(XCLBIN_PATH)" \
		--host-bin "$(HOST_BIN)" \
		--dataset "$(DATASET)"
	rtk $(MAKE) compare DATASET=$(DATASET)

test-slow: configure
	rtk ctest --test-dir $(BUILD_DIR) -L "csynth|cosim|xrt_emu" -V
	@rtk $(PYTHON) -m pytest -m slow tests/python -v; status=$$?; if [ $$status -eq 5 ]; then rtk echo "No slow Python tests selected"; elif [ $$status -ne 0 ]; then exit $$status; fi

test-all: test test-slow

clean:
	rtk rm -rf $(BUILD_DIR)
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		rtk rm -rf $(HOST_BUILD_DIR); \
	fi

clean-all:
	rtk rm -rf build/ *.egg-info python/anvil.egg-info

deploy-check:
	@if [ -z "$(BOARD_IP)" ]; then rtk echo "ERROR: BOARD_IP not set. Usage: make deploy BOARD_IP=<ip> [BOARD_SSH_USER=root] [DATASET=tiny]" >&2; exit 1; fi

deploy-bin: deploy-check
	@if [ ! -f "$(HOST_BIN)" ]; then rtk echo "ERROR: $(HOST_BIN) not found. Run: make build TARGET=$(TARGET) first." >&2; exit 1; fi
	rtk ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	rtk scp "$(HOST_BIN)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/run_saxpy"

deploy-xclbin: deploy-check
	@if [ ! -f "$(XCLBIN_PATH)" ]; then rtk echo "ERROR: $(XCLBIN_PATH) not found. Run: make xclbin TARGET=$(TARGET) first." >&2; exit 1; fi
	rtk ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	rtk scp "$(XCLBIN_PATH)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/saxpy.xclbin"

deploy-data: deploy-check
	@if [ ! -d "data/$(DATASET)" ]; then rtk echo "ERROR: data/$(DATASET) not found. Run: make gen DATASET=$(DATASET) first." >&2; exit 1; fi
	rtk ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)/data/$(DATASET)"
	rtk scp -r "data/$(DATASET)/." "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/data/$(DATASET)/"

deploy: deploy-bin deploy-xclbin deploy-data
	@rtk echo "[deploy] complete: $(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)"

help:
	@rtk echo "Targets:"
	@rtk echo "  make build [TARGET=u250|zcu104|zcu102]  — configure + build C++ + pip install"
	@rtk echo "  make test                         — CPU-only fast tests (no Vitis/XRT)"
	@rtk echo "  make csynth                       — v++ HLS synthesis"
	@rtk echo "  make cosim                        — HLS co-simulation"
	@rtk echo "  make xclbin                       — link .xclbin"
	@rtk echo "  make gen                          — generate dataset"
	@rtk echo "  make gold [ANVIL_LANG=cpp|python]       — run gold reference"
	@rtk echo "  make xrt-emu                      — run accelerator hw_emu or build embedded + print QEMU note"
	@rtk echo "  make xrt-hw                       — run on real hardware"
	@rtk echo "  make compare                      — compare outputs vs gold"
	@rtk echo "  make analyze                      — parse HLS csynth report"
	@rtk echo "  make emconfig                     — generate emconfig.json for hw_emu"
	@rtk echo "  make test-csynth/cosim/xrt-emu    — label-specific hardware tests"
	@rtk echo "  make clean [TARGET=...]           — remove preset build dir"
	@rtk echo "  make clean-all                    — remove all build dirs + eggs"
	@rtk echo "  make deploy [TARGET=zcu102] BOARD_IP=<ip>     — scp bin+xclbin+data to board"
	@rtk echo "  make deploy-bin/xclbin/data BOARD_IP=<ip>     — deploy individual artifact"
	@rtk echo "  make test-xrt-hw BOARD_IP=<ip> [DATASET=tiny] — build+xclbin+gen+deploy+run+compare on board"
