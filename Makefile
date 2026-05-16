# ============================================================================
# Vitis-Anvil top-level Makefile
# Vitis-Anvil 顶层 Makefile
# ============================================================================
# Usage / 使用方法:
#   make [target] [TARGET=u250|u55c|zcu104|zcu102|kv260] [ANVIL_LANG=cpp|python] [DATASET=tiny]

# --- User-configurable variables / 用户可配置变量 ---
# Target board / 目标板卡
TARGET  ?= u250
# Language for gold reference / 黄金参考使用的语言 (cpp / python)
ANVIL_LANG ?= cpp
# Dataset size / 数据集大小
DATASET ?= tiny
# Kernel filter for analysis / 分析时筛选的 kernel 名称
KERNEL ?= all
HOST_APP ?= run_saxpy

# Include board-specific configuration / 引入板卡专用配置
include config/$(TARGET)/anvil.mk

ifeq ($(HOST_APP),run_pipeline_demo)
XCLBIN_NAME := pipeline_demo
else
XCLBIN_NAME := saxpy
endif
ifeq ($(HOST_APP),run_vadd)
COMPARE_AFTER_HW ?= no
else ifeq ($(HOST_APP),run_pipeline_demo)
COMPARE_AFTER_HW ?= no
else
COMPARE_AFTER_HW ?= yes
endif

# --- Derived variables / 派生变量 ---
# CMake command-line overrides derived from config/<TARGET>/anvil.mk.
# This lets users override ANVIL_PLATFORM=/path/to/platform.xpfm from make.
CMAKE_PLATFORM_ARGS := -DANVIL_VITIS_PLATFORM=$(ANVIL_PLATFORM) -DANVIL_VITIS_PART=$(ANVIL_VITIS_PART)
# Build directory for the selected preset / 当前 preset 的构建目录
BUILD_DIR   := build/$(ANVIL_PRESET)
# CMake targets for HLS synthesis / HLS 综合的 CMake target
ANVIL_KERNEL_TARGETS ?= saxpy_xo
ifeq ($(KERNEL),all)
SELECTED_KERNEL_TARGETS := $(ANVIL_KERNEL_TARGETS)
else ifeq ($(KERNEL),pipeline_demo)
SELECTED_KERNEL_TARGETS := saxpy_stream_xo vadd_stream_xo
else
SELECTED_KERNEL_TARGETS := $(KERNEL)_xo
endif
# CMake targets for co-simulation / 协同仿真的 CMake target
ANVIL_COSIM_TARGETS ?= saxpy_cosim vadd_cosim
ifeq ($(KERNEL),all)
SELECTED_COSIM_TARGETS := $(ANVIL_COSIM_TARGETS)
else ifeq ($(KERNEL),pipeline_demo)
SELECTED_COSIM_TARGETS := saxpy_stream_cosim vadd_stream_cosim
else
SELECTED_COSIM_TARGETS := $(KERNEL)_cosim
endif
# Host (cross-compilation) preset / 主机端（交叉编译）preset
ANVIL_HOST_PRESET  ?= $(ANVIL_PRESET)
HOST_BUILD_DIR     := build/$(ANVIL_HOST_PRESET)
HOST_BIN    := $(HOST_BUILD_DIR)/src/host/$(HOST_APP)
XCLBIN_PATH := $(BUILD_DIR)/src/kernels/$(XCLBIN_NAME)_xclbin/$(XCLBIN_NAME).xclbin
# Hardware emulation preset / 硬件仿真 preset
ANVIL_HWEMU_PRESET ?= $(ANVIL_PRESET)-hwemu
HWEMU_BUILD_DIR    := build/$(ANVIL_HWEMU_PRESET)
HWEMU_HOST_BIN     := $(HWEMU_BUILD_DIR)/src/host/$(HOST_APP)
HWEMU_XCLBIN_PATH  := $(HWEMU_BUILD_DIR)/src/kernels/$(XCLBIN_NAME)_xclbin/$(XCLBIN_NAME).xclbin
# Python venv / Python 虚拟环境
PYTHON      := .venv/bin/python
PIP         := $(PYTHON) -m pip
VENV_STAMP  := .venv/.anvil-install.stamp
PYPI_INDEX  ?= https://mirrors.ustc.edu.cn/pypi/simple
PIP_INDEX_ARGS := $(if $(PYPI_INDEX),-i $(PYPI_INDEX),)
# HLS flow Python entry / HLS 流程 Python 入口
HLSFLOW_PYTHON := PYTHONPATH=tools $(PYTHON)
# Deployment to physical board / 部署到物理板卡
BOARD_IP          ?=
BOARD_SSH_USER    ?= root
BOARD_DEPLOY_DIR  ?= ~/anvil-deploy

# --- Phony targets declaration / 伪目标声明 ---
.PHONY: all configure build build-cpp build-python python-env python-install rebuild-python require-python-env clean clean-all help help-en help-zh \
        configure-kernel configure-host build-host build-kernel build-all \
        csynth cosim xclbin xclbin-hwemu \
        require-u250-stream csynth-stream cosim-stream pipeline-demo \
        gen gold run-host xrt-emu xrt-hw compare analyze analyze-legacy analyze-flow analyze-cosim check-hls compare-hls emconfig \
        deploy deploy-bin deploy-xclbin deploy-data deploy-check \
        test test-csynth test-cosim test-xrt-emu test-xrt-hw test-slow test-all

# ============================================================================
# Top-level targets / 顶层目标
# ============================================================================

# Default target: fast host/Python build / 默认目标：快速 host/Python 构建
all: build

# --------------------------------------------------------------------------
# configure — configure both kernel and host presets / 同时配置 kernel 与 host preset
# Kept as an explicit aggregate for users that want both sides. Fast targets below
# call configure-kernel/configure-host directly to avoid accidental HLS synthesis.
# --------------------------------------------------------------------------
configure: configure-kernel configure-host

configure-kernel:
	cmake --preset $(ANVIL_PRESET) $(CMAKE_PLATFORM_ARGS)

configure-host:
	@if [ -n "$(ANVIL_SYSROOT)" ]; then \
		env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	else \
		cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	fi

# --------------------------------------------------------------------------
# build — fast day-to-day build: host binary only, no Python env, no HLS synthesis
#         日常快速构建：只构建 host 程序，不创建 Python 环境，不触发 HLS 综合
# --------------------------------------------------------------------------
build: build-host

# Explicit full build for users who really want kernel + host + Python.
# 明确的完整构建：需要 kernel + host + Python 时手动调用。
build-all: build-kernel build-host

# Backward-compatible C++ build alias: host-side C++ only, no HLS synthesis.
build-cpp: build-host

build-host: configure-host
	cmake --build --preset $(ANVIL_HOST_PRESET) --target $(HOST_APP)

build-kernel: configure-kernel
	cmake --build --preset $(ANVIL_PRESET) --target $(SELECTED_KERNEL_TARGETS)

# Python environment is explicit: normal `make build` does not create/update it.
# Defaults to USTC PyPI mirror; disable with `make python-env PYPI_INDEX=`.
build-python: python-env

python-env: $(VENV_STAMP)

$(VENV_STAMP): pyproject.toml
	@if command -v uv >/dev/null 2>&1; then \
		uv venv .venv; \
		uv pip install $(PIP_INDEX_ARGS) -e ".[test]" --python $(PYTHON); \
	else \
		test -x $(PYTHON) || python3 -m venv .venv; \
		$(PIP) install $(PIP_INDEX_ARGS) -e ".[test]" --quiet; \
	fi
	@touch $(VENV_STAMP)

python-install: python-env

rebuild-python:
	rm -rf .venv
	@if command -v uv >/dev/null 2>&1; then \
		uv venv .venv; \
		uv pip install $(PIP_INDEX_ARGS) -e ".[test]" --python $(PYTHON); \
	else \
		python3 -m venv .venv; \
		$(PIP) install $(PIP_INDEX_ARGS) -e ".[test]" --quiet; \
	fi
	@touch $(VENV_STAMP)

require-python-env:
	@if [ ! -x "$(PYTHON)" ]; then \
		echo "ERROR: Python environment missing. Run: make python-env" >&2; \
		exit 1; \
	fi

# ============================================================================
# HLS / Vitis targets / HLS / Vitis 目标
# ============================================================================

# csynth — Run HLS synthesis on kernel(s) / 对内核运行 HLS 综合
csynth: build-kernel

# cosim — Run HLS co-simulation / 运行 HLS 协同仿真
cosim:
	@if [ "$(KERNEL)" = "pipeline_demo" ] && [ "$(TARGET)" != "u250" ]; then echo "KERNEL=pipeline_demo cosim currently requires TARGET=u250" >&2; exit 1; fi
	@if [ -z "$(strip $(SELECTED_COSIM_TARGETS))" ]; then echo "cosim is not configured for TARGET=$(TARGET)" >&2; exit 1; fi
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target $(SELECTED_COSIM_TARGETS)

# xclbin — Link kernel into .xclbin bitstream / 将内核链接为 .xclbin 比特流
xclbin: configure-kernel
	cmake --build $(BUILD_DIR) --target $(XCLBIN_NAME)_xclbin

# xclbin-hwemu — Build xclbin for hardware emulation / 构建硬件仿真用的 xclbin
xclbin-hwemu:
	cmake --preset $(ANVIL_HWEMU_PRESET) $(CMAKE_PLATFORM_ARGS)
	cmake --build --preset $(ANVIL_HWEMU_PRESET) --target $(XCLBIN_NAME)_xclbin

# --------------------------------------------------------------------------
# U250 streaming kernel targets (require TARGET=u250)
# U250 流式内核目标（需要 TARGET=u250）
# --------------------------------------------------------------------------
require-u250-stream:
	@if [ "$(TARGET)" != "u250" ]; then \
		echo "error: csynth-stream / cosim-stream / pipeline-demo require TARGET=u250 (got TARGET=$(TARGET))" >&2; exit 1; \
	fi

csynth-stream:
	$(MAKE) require-u250-stream TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_xo vadd_stream_xo

cosim-stream:
	$(MAKE) require-u250-stream TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_cosim vadd_stream_cosim

pipeline-demo:
	$(MAKE) require-u250-stream TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target pipeline_demo_xclbin

# ============================================================================
# Data generation and golden reference / 数据生成与黄金参考
# ============================================================================

# gen — Generate dataset / 生成数据集
gen:
	@if [ ! -f scripts/gen_dataset.py ]; then echo "scripts/gen_dataset.py is added in Task 15" >&2; exit 1; fi
	$(MAKE) build-python
	$(PYTHON) scripts/gen_dataset.py --dataset $(DATASET)

# gold — Run golden reference to produce expected output / 运行黄金参考生成期望输出
gold: gen
	@if [ ! -f scripts/run_gold.sh ]; then echo "scripts/run_gold.sh is added in Task 15" >&2; exit 1; fi
	@if [ "$(ANVIL_LANG)" = "cpp" ]; then \
		$(MAKE) configure-host TARGET=$(TARGET); \
		cmake --build --preset $(ANVIL_HOST_PRESET) --target saxpy_gold_bin; \
	fi
	env ANVIL_LANG=$(ANVIL_LANG) DATASET=$(DATASET) ANVIL_PRESET=$(ANVIL_HOST_PRESET) bash scripts/run_gold.sh

# ============================================================================
# Run on hardware / 在硬件上运行
# ============================================================================

# run-host — Execute host binary on real hardware / 在真实硬件上执行主机程序
run-host:
	@if [ "$(HOST_APP)" = "run_pipeline_demo" ] && [ "$(TARGET)" != "u250" ]; then echo "HOST_APP=run_pipeline_demo currently requires TARGET=u250" >&2; exit 1; fi
	$(MAKE) build-host TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	$(MAKE) gen DATASET=$(DATASET)
	@if [ ! -x $(HOST_BIN) ]; then echo "$(HOST_BIN) not built; use a preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(XCLBIN_PATH) ]; then echo "$(XCLBIN_PATH) not found; run make xclbin first" >&2; exit 1; fi
	$(HOST_BIN) --xclbin $(XCLBIN_PATH) --data-dir data/$(DATASET) --output data/$(DATASET)/xrt_hw_out.bin

# emconfig — Generate emconfig.json for hw_emu / 为硬件仿真生成 emconfig.json
emconfig:
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh

# xrt-emu — Run in hardware emulation / 在硬件仿真中运行
# Handles both embedded (AArch64+QEMU) and accelerator (x86 hw_emu) flows
# 同时支持嵌入式（AArch64+QEMU）和加速器（x86 hw_emu）流程
ifeq ($(ANVIL_DEVICE_KIND),embedded)
xrt-emu: gen
	echo "[xrt-emu] TARGET=$(TARGET): building kernel ($(ANVIL_PRESET)) + AArch64 host ($(ANVIL_HOST_PRESET))..."
	cmake --preset $(ANVIL_PRESET) $(CMAKE_PLATFORM_ARGS)
	cmake --build --preset $(ANVIL_PRESET) --target $(XCLBIN_NAME)_xclbin
	@if [ -n "$(ANVIL_SYSROOT)" ]; then \
		env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	else \
		cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	fi
	cmake --build --preset $(ANVIL_HOST_PRESET) --target $(HOST_APP)
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	echo ""
	echo "Embedded hw_emu requires QEMU — see docs/deploy.md#qemu-emulation"
	echo "  xclbin : $(XCLBIN_PATH)"
	echo "  host   : $(HOST_BIN)  (AArch64)"
	echo "  emcfg  : $(HWEMU_BUILD_DIR)/emconfig.json"
else
xrt-emu: gen
	@if [ "$(ANVIL_DEVICE_KIND)" != "accelerator" ]; then echo "make xrt-emu currently requires an accelerator target with a combined host+kernel hw_emu preset; got TARGET=$(TARGET) ($(ANVIL_DEVICE_KIND))" >&2; exit 1; fi
	cmake --preset $(ANVIL_HWEMU_PRESET) $(CMAKE_PLATFORM_ARGS)
	cmake --build --preset $(ANVIL_HWEMU_PRESET) --target $(HOST_APP) $(XCLBIN_NAME)_xclbin
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	@if [ ! -x $(HWEMU_HOST_BIN) ]; then echo "$(HWEMU_HOST_BIN) not built; use a hw_emu preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(HWEMU_XCLBIN_PATH) ]; then echo "$(HWEMU_XCLBIN_PATH) not found; make xrt-emu should have built $(XCLBIN_NAME)_xclbin" >&2; exit 1; fi
	env XCL_EMULATION_MODE=hw_emu EMCONFIG_PATH=$(HWEMU_BUILD_DIR) $(HWEMU_HOST_BIN) --xclbin $(HWEMU_XCLBIN_PATH) --data-dir data/$(DATASET) --output data/$(DATASET)/xrt_emu_out.bin
endif

# xrt-hw — Run on real hardware (alias for run-host) / 在真实硬件上运行（run-host 的别名）
xrt-hw:
	$(MAKE) run-host TARGET=$(TARGET) HOST_APP=$(HOST_APP) DATASET=$(DATASET)

# ============================================================================
# Analysis and comparison / 分析与对比
# ============================================================================

# compare — Compare output against golden reference / 将输出与黄金参考对比
compare:
	@if [ ! -f scripts/compare.py ]; then echo "scripts/compare.py is added in Task 16" >&2; exit 1; fi
	$(MAKE) build-python
	$(PYTHON) scripts/compare.py --dataset $(DATASET)

# analyze — Deep HLS flow analysis via hlsflow / 通过 hlsflow 进行深度 HLS 流程分析
analyze: require-python-env
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(KERNEL) --target csynth --platform $(TARGET)

# analyze-flow — Backward-compatible alias / 兼容旧入口
analyze-flow: analyze

# analyze-cosim — Collect cosim report into hlsflow database
analyze-cosim: require-python-env
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(KERNEL) --target cosim --platform $(TARGET)

# analyze-legacy — Old simple parser / 旧版简易解析器
analyze-legacy: require-python-env
	@if [ ! -f scripts/analyze.py ]; then echo "scripts/analyze.py is added in Task 16" >&2; exit 1; fi
	$(PYTHON) scripts/analyze.py --build-dir $(BUILD_DIR)

# check-hls — Run HLS threshold checks / 运行 HLS 阈值检查
check-hls: require-python-env
	env $(HLSFLOW_PYTHON) -m hlsflow check --max-ii 1

# compare-hls — Diff two HLS runs / 对比两次 HLS 运行结果
compare-hls:
	@if [ -z "$(BASELINE)" ] || [ -z "$(CANDIDATE)" ]; then echo "Usage: make compare-hls BASELINE=<id> CANDIDATE=<id>" >&2; exit 1; fi
	$(MAKE) build-python
	env $(HLSFLOW_PYTHON) -m hlsflow compare --baseline "$(BASELINE)" --candidate "$(CANDIDATE)"

# ============================================================================
# Testing / 测试
# ============================================================================

# test — CPU-only fast tests (no Vitis/XRT), using hls-model-linux-debug preset
#        仅 CPU 的快速测试（无需 Vitis/XRT），使用 hls-model-linux-debug preset
test:
	cmake --preset hls-model-linux-debug
	cmake --build --preset hls-model-linux-debug
	ctest --preset hls-model-linux-debug
	$(MAKE) build-python
	$(PYTHON) -m pytest -m fast tests/python -v

# Label-specific CTest targets / 按标签筛选的 CTest 目标
test-csynth: configure-kernel
	ctest --test-dir $(BUILD_DIR) -L csynth -V

test-cosim:
	$(MAKE) cosim TARGET=$(TARGET) KERNEL=$(KERNEL)

test-xrt-emu:
	$(MAKE) xrt-emu

# test-xrt-hw — Full end-to-end hardware test / 完整的端到端硬件测试
# Steps / 步骤: build (cross-compile + xclbin) → gen → deploy+run via board_run.py → compare
# Requires / 需要: BOARD_IP and (for embedded targets) PETALINUX_SYSROOT + Vitis env
test-xrt-hw:
	@if [ -z "$(BOARD_IP)" ]; then echo "ERROR: BOARD_IP not set. Usage: make test-xrt-hw BOARD_IP=<ip> [TARGET=zcu102|kv260] [DATASET=tiny]" >&2; exit 1; fi
	$(MAKE) build-host TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	$(MAKE) xclbin TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	$(MAKE) gen DATASET=$(DATASET)
	$(MAKE) build-python
	$(PYTHON) scripts/board_run.py \
		--board-ip "$(BOARD_IP)" \
		--ssh-user "$(BOARD_SSH_USER)" \
		--deploy-dir "$(BOARD_DEPLOY_DIR)" \
		--xclbin "$(XCLBIN_PATH)" \
		--host-bin "$(HOST_BIN)" \
		--host-app "$(HOST_APP)" \
		--xclbin-name "$(XCLBIN_NAME)" \
		--dataset "$(DATASET)"
	@if [ "$(COMPARE_AFTER_HW)" = "yes" ]; then $(MAKE) compare DATASET=$(DATASET); else echo "Skipping make compare for HOST_APP=$(HOST_APP)"; fi

# test-slow — Run all slow hardware tests + slow Python tests / 运行所有慢速硬件测试和 Python 测试
test-slow: configure
	ctest --test-dir $(BUILD_DIR) -L "csynth|cosim|xrt_emu" -V
	@$(PYTHON) -m pytest -m slow tests/python -v; status=$$?; if [ $$status -eq 5 ]; then echo "No slow Python tests selected"; elif [ $$status -ne 0 ]; then exit $$status; fi

# test-all — Run all tests (fast + slow) / 运行所有测试（快速 + 慢速）
test-all: test test-slow

# ============================================================================
# Cleanup / 清理
# ============================================================================

# clean — Remove build directory for the current preset / 移除当前 preset 的构建目录
clean:
	rm -rf $(BUILD_DIR)
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		rm -rf $(HOST_BUILD_DIR); \
	fi

# clean-all — Remove all build artifacts / 移除所有构建产物
clean-all:
	rm -rf build/ *.egg-info python/anvil.egg-info

# ============================================================================
# Deployment to physical board via SSH / 通过 SSH 部署到物理板卡
# ============================================================================

deploy-check:
	@if [ -z "$(BOARD_IP)" ]; then echo "ERROR: BOARD_IP not set. Usage: make deploy BOARD_IP=<ip> [BOARD_SSH_USER=root] [DATASET=tiny]" >&2; exit 1; fi

# deploy-bin — Deploy the host binary / 部署主机端二进制文件
deploy-bin: deploy-check
	@if [ ! -f "$(HOST_BIN)" ]; then echo "ERROR: $(HOST_BIN) not found. Run: make build TARGET=$(TARGET) HOST_APP=$(HOST_APP) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	scp "$(HOST_BIN)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/$(HOST_APP)"

# deploy-xclbin — Deploy the .xclbin bitstream / 部署 .xclbin 比特流
deploy-xclbin: deploy-check
	@if [ ! -f "$(XCLBIN_PATH)" ]; then echo "ERROR: $(XCLBIN_PATH) not found. Run: make xclbin TARGET=$(TARGET) HOST_APP=$(HOST_APP) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	scp "$(XCLBIN_PATH)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/$(XCLBIN_NAME).xclbin"

# deploy-data — Deploy the dataset / 部署数据集
deploy-data: deploy-check
	@if [ ! -d "data/$(DATASET)" ]; then echo "ERROR: data/$(DATASET) not found. Run: make gen DATASET=$(DATASET) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)/data/$(DATASET)"
	scp -r "data/$(DATASET)/." "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/data/$(DATASET)/"

# deploy — Deploy everything (bin + xclbin + data) / 部署全部（二进制 + 比特流 + 数据）
deploy: deploy-bin deploy-xclbin deploy-data
	@echo "[deploy] complete: $(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)"

# ============================================================================
# Help / 帮助
# ============================================================================

help: help-en

help-en:
	@echo "Vitis-Anvil — A CMake project template for forging Vitis/XRT FPGA accelerators."
	@echo ""
	@echo "Usage:"
	@echo "  make <target> [TARGET=u250] [KERNEL=saxpy|vadd|all] [HOST_APP=run_saxpy] [DATASET=tiny]"
	@echo ""
	@echo "Core variables:"
	@echo "  TARGET              Board/platform: u250,u50,u55c,u200,u280,vck5000,zcu102,zcu104,zcu106,kv260"
	@echo "  KERNEL              HLS kernel selection for csynth/cosim/analyze"
	@echo "  HOST_APP            XRT host program: run_saxpy, run_vadd, run_pipeline_demo"
	@echo "  DATASET             Dataset directory under data/"
	@echo "  ANVIL_PLATFORM      Override platform .xpfm path"
	@echo "  PETALINUX_SYSROOT   Embedded AArch64 sysroot"
	@echo ""
	@echo "Fast local development:"
	@echo "  make test                                      CPU-only tests; no Vitis/XRT/platform"
	@echo "  make build TARGET=u250 HOST_APP=run_saxpy     Build selected host binary only"
	@echo "  make python-env [PYPI_INDEX=]                  Create/update .venv; uv first, venv fallback"
	@echo "  make rebuild-python [PYPI_INDEX=]              Recreate .venv"
	@echo ""
	@echo "HLS kernel flow:"
	@echo "  make csynth TARGET=u250 KERNEL=saxpy           Run Vitis HLS synthesis"
	@echo "  make cosim TARGET=u250 KERNEL=saxpy            Run HLS C/RTL cosimulation"
	@echo "  make analyze-flow TARGET=u250 KERNEL=saxpy     Analyze csynth reports"
	@echo "  make analyze-cosim TARGET=u250 KERNEL=saxpy    Analyze cosim reports"
	@echo "  make check-hls                                 Check latest HLS run thresholds"
	@echo "  make compare-hls BASELINE=<id> CANDIDATE=<id>  Compare two HLS runs"
	@echo ""
	@echo "XRT / hardware flow:"
	@echo "  make gen DATASET=tiny                          Generate input dataset"
	@echo "  make gold DATASET=tiny ANVIL_LANG=cpp          Generate gold output"
	@echo "  make xclbin TARGET=u250                        Link hardware xclbin; long-running"
	@echo "  make xclbin-hwemu TARGET=u250                  Link hw_emu xclbin"
	@echo "  make xrt-emu TARGET=u250 HOST_APP=run_saxpy    Run accelerator hw_emu or print embedded QEMU note"
	@echo "  make run-host TARGET=u250 HOST_APP=run_saxpy   Run selected host app on real hardware"
	@echo "  make xrt-hw TARGET=u250 HOST_APP=run_saxpy     Alias for run-host"
	@echo "  make compare DATASET=tiny                      Compare output with gold"
	@echo ""
	@echo "Embedded deployment flow:"
	@echo "  PETALINUX_SYSROOT=/path make build-host TARGET=zcu102 HOST_APP=run_saxpy"
	@echo "  make xclbin TARGET=zcu102"
	@echo "  make deploy TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make test-xrt-hw TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make deploy-bin|deploy-xclbin|deploy-data TARGET=zcu102 BOARD_IP=<ip>"
	@echo ""
	@echo "U250 stream demo:"
	@echo "  make csynth-stream TARGET=u250"
	@echo "  make cosim-stream TARGET=u250"
	@echo "  make pipeline-demo TARGET=u250"
	@echo "  make run-host TARGET=u250 HOST_APP=run_pipeline_demo"
	@echo ""
	@echo "Maintenance / compatibility aliases:"
	@echo "  make configure|configure-kernel|configure-host  Configure CMake presets"
	@echo "  make build-kernel|build-all|build-cpp           Build aliases"
	@echo "  make analyze|analyze-legacy|analyze-flow        Analysis entrypoints"
	@echo "  make test-csynth|test-cosim|test-xrt-emu        Label-specific tests"
	@echo "  make clean|clean-all                            Remove build artifacts"
	@echo "  make help-zh                                    Chinese help"
	@echo ""
	@echo "Docs: README.en.md, README.zh.md, docs/en/get_started.md, docs/zh/get_started.md"

help-zh:
	@echo "Vitis-Anvil — 用于锻造 Vitis/XRT FPGA accelerators 的 CMake 工程模板。"
	@echo ""
	@echo "用法:"
	@echo "  make <target> [TARGET=u250] [KERNEL=saxpy|vadd|all] [HOST_APP=run_saxpy] [DATASET=tiny]"
	@echo ""
	@echo "核心变量:"
	@echo "  TARGET              板卡/platform: u250,u50,u55c,u200,u280,vck5000,zcu102,zcu104,zcu106,kv260"
	@echo "  KERNEL              HLS kernel 选择，用于 csynth/cosim/analyze"
	@echo "  HOST_APP            XRT host 程序: run_saxpy, run_vadd, run_pipeline_demo"
	@echo "  DATASET             data/ 下的数据集目录名"
	@echo "  ANVIL_PLATFORM      覆盖 platform .xpfm 路径"
	@echo "  PETALINUX_SYSROOT   Embedded AArch64 sysroot"
	@echo ""
	@echo "快速本地开发:"
	@echo "  make test                                      CPU-only 测试；不需要 Vitis/XRT/platform"
	@echo "  make build TARGET=u250 HOST_APP=run_saxpy     只构建选定 host binary"
	@echo "  make python-env [PYPI_INDEX=]                  创建/更新 .venv；优先 uv，fallback venv"
	@echo "  make rebuild-python [PYPI_INDEX=]              重建 .venv"
	@echo ""
	@echo "HLS kernel 流程:"
	@echo "  make csynth TARGET=u250 KERNEL=saxpy           运行 Vitis HLS synthesis"
	@echo "  make cosim TARGET=u250 KERNEL=saxpy            运行 HLS C/RTL cosimulation"
	@echo "  make analyze-flow TARGET=u250 KERNEL=saxpy     分析 csynth 报告"
	@echo "  make analyze-cosim TARGET=u250 KERNEL=saxpy    分析 cosim 报告"
	@echo "  make check-hls                                 检查最新 HLS run 阈值"
	@echo "  make compare-hls BASELINE=<id> CANDIDATE=<id>  对比两次 HLS run"
	@echo ""
	@echo "XRT / 硬件流程:"
	@echo "  make gen DATASET=tiny                          生成输入数据"
	@echo "  make gold DATASET=tiny ANVIL_LANG=cpp          生成 gold output"
	@echo "  make xclbin TARGET=u250                        链接硬件 xclbin；耗时较长"
	@echo "  make xclbin-hwemu TARGET=u250                  链接 hw_emu xclbin"
	@echo "  make xrt-emu TARGET=u250 HOST_APP=run_saxpy    跑加速卡 hw_emu；embedded 打印 QEMU 提示"
	@echo "  make run-host TARGET=u250 HOST_APP=run_saxpy   在真实硬件上运行选定 host app"
	@echo "  make xrt-hw TARGET=u250 HOST_APP=run_saxpy     run-host 别名"
	@echo "  make compare DATASET=tiny                      和 gold 比较输出"
	@echo ""
	@echo "Embedded 部署流程:"
	@echo "  PETALINUX_SYSROOT=/path make build-host TARGET=zcu102 HOST_APP=run_saxpy"
	@echo "  make xclbin TARGET=zcu102"
	@echo "  make deploy TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make test-xrt-hw TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make deploy-bin|deploy-xclbin|deploy-data TARGET=zcu102 BOARD_IP=<ip>"
	@echo ""
	@echo "U250 stream demo:"
	@echo "  make csynth-stream TARGET=u250"
	@echo "  make cosim-stream TARGET=u250"
	@echo "  make pipeline-demo TARGET=u250"
	@echo "  make run-host TARGET=u250 HOST_APP=run_pipeline_demo"
	@echo ""
	@echo "维护 / 兼容别名:"
	@echo "  make configure|configure-kernel|configure-host  配置 CMake presets"
	@echo "  make build-kernel|build-all|build-cpp           构建别名"
	@echo "  make analyze|analyze-legacy|analyze-flow        分析入口"
	@echo "  make test-csynth|test-cosim|test-xrt-emu        按标签/流程测试"
	@echo "  make clean|clean-all                            清理构建产物"
	@echo "  make help-en                                    英文帮助"
	@echo ""
	@echo "文档: README.en.md, README.zh.md, docs/en/get_started.md, docs/zh/get_started.md"
