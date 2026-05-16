# ============================================================================
# Vitis-Anvil top-level Makefile
# Vitis-Anvil 顶层 Makefile
# ============================================================================
# Usage / 使用方法:
#   make [target] [TARGET=u250|u55c|zcu104|zcu102] [ANVIL_LANG=cpp|python] [DATASET=tiny]

# --- User-configurable variables / 用户可配置变量 ---
# Target board / 目标板卡
TARGET  ?= u250
# Language for gold reference / 黄金参考使用的语言 (cpp / python)
ANVIL_LANG ?= cpp
# Dataset size / 数据集大小
DATASET ?= tiny
# Kernel filter for analysis / 分析时筛选的 kernel 名称
KERNEL ?= all

# Include board-specific configuration / 引入板卡专用配置
include config/$(TARGET)/anvil.mk

# --- Derived variables / 派生变量 ---
# Build directory for the selected preset / 当前 preset 的构建目录
BUILD_DIR   := build/$(ANVIL_PRESET)
# CMake targets for HLS synthesis / HLS 综合的 CMake target
ANVIL_KERNEL_TARGETS ?= saxpy_xo
# CMake targets for co-simulation / 协同仿真的 CMake target
ANVIL_COSIM_TARGETS ?= saxpy_cosim
# Host (cross-compilation) preset / 主机端（交叉编译）preset
ANVIL_HOST_PRESET  ?= $(ANVIL_PRESET)
HOST_BUILD_DIR     := build/$(ANVIL_HOST_PRESET)
HOST_BIN    := $(HOST_BUILD_DIR)/src/host/run_saxpy
XCLBIN_PATH := $(BUILD_DIR)/src/kernels/saxpy_xclbin/saxpy.xclbin
# Hardware emulation preset / 硬件仿真 preset
ANVIL_HWEMU_PRESET ?= $(ANVIL_PRESET)-hwemu
HWEMU_BUILD_DIR    := build/$(ANVIL_HWEMU_PRESET)
HWEMU_HOST_BIN     := $(HWEMU_BUILD_DIR)/src/host/run_saxpy
HWEMU_XCLBIN_PATH  := $(HWEMU_BUILD_DIR)/src/kernels/saxpy_xclbin/saxpy.xclbin
# Python venv / Python 虚拟环境
PYTHON      := .venv/bin/python
PIP         := $(PYTHON) -m pip
# HLS flow Python entry / HLS 流程 Python 入口
HLSFLOW_PYTHON := PYTHONPATH=tools $(PYTHON)
# Deployment to physical board / 部署到物理板卡
BOARD_IP          ?=
BOARD_SSH_USER    ?= root
BOARD_DEPLOY_DIR  ?= ~/anvil-deploy

# --- Phony targets declaration / 伪目标声明 ---
.PHONY: all configure build build-cpp build-python clean clean-all help \
        csynth cosim xclbin xclbin-hwemu \
        require-u250-stream csynth-stream cosim-stream pipeline-demo \
        gen gold run-host xrt-emu xrt-hw compare analyze analyze-flow check-hls compare-hls emconfig \
        deploy deploy-bin deploy-xclbin deploy-data deploy-check \
        test test-csynth test-cosim test-xrt-emu test-xrt-hw test-slow test-all

# ============================================================================
# Top-level targets / 顶层目标
# ============================================================================

# Default target: build everything / 默认目标：构建所有
all: build

# --------------------------------------------------------------------------
# configure — CMake configure step / CMake 配置步骤
# --------------------------------------------------------------------------
configure:
	cmake --preset $(ANVIL_PRESET)
	@# If there is a separate host (cross-compile) preset, configure it too
	@# 如果有独立的主机端（交叉编译）preset，也一并配置
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		if [ -n "$(ANVIL_SYSROOT)" ]; then \
			env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET); \
		else \
			cmake --preset $(ANVIL_HOST_PRESET); \
		fi; \
	fi

# --------------------------------------------------------------------------
# build — configure + build C++ host/kernel + install Python package
#         配置 + 构建 C++ 主机/内核 + 安装 Python 包
# --------------------------------------------------------------------------
build: configure build-cpp build-python

# Build C++ code (kernels and host) / 构建 C++ 代码（内核和主机）
build-cpp:
	cmake --build --preset $(ANVIL_PRESET)
	@if [ "$(ANVIL_HOST_PRESET)" != "$(ANVIL_PRESET)" ]; then \
		cmake --build --preset $(ANVIL_HOST_PRESET); \
	fi

# Build Python venv and install the package / 构建 Python 虚拟环境并安装包
build-python:
	rm -rf .venv
	python3 -m venv .venv
	$(PIP) install -e ".[test]" --quiet

# ============================================================================
# HLS / Vitis targets / HLS / Vitis 目标
# ============================================================================

# csynth — Run HLS synthesis on kernel(s) / 对内核运行 HLS 综合
csynth: configure
	cmake --build $(BUILD_DIR) --target $(ANVIL_KERNEL_TARGETS)

# cosim — Run HLS co-simulation / 运行 HLS 协同仿真
cosim:
	@if [ -z "$(strip $(ANVIL_COSIM_TARGETS))" ]; then echo "cosim is not configured for TARGET=$(TARGET)" >&2; exit 1; fi
	$(MAKE) configure TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target $(ANVIL_COSIM_TARGETS)

# xclbin — Link kernel into .xclbin bitstream / 将内核链接为 .xclbin 比特流
xclbin: configure
	cmake --build $(BUILD_DIR) --target saxpy_xclbin

# xclbin-hwemu — Build xclbin for hardware emulation / 构建硬件仿真用的 xclbin
xclbin-hwemu:
	cmake --preset $(ANVIL_HWEMU_PRESET)
	cmake --build --preset $(ANVIL_HWEMU_PRESET) --target saxpy_xclbin

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
	$(MAKE) configure TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_xo vadd_stream_xo

cosim-stream:
	$(MAKE) require-u250-stream TARGET=$(TARGET)
	$(MAKE) configure TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_cosim vadd_stream_cosim

pipeline-demo:
	$(MAKE) require-u250-stream TARGET=$(TARGET)
	$(MAKE) configure TARGET=$(TARGET)
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
gold: build gen
	@if [ ! -f scripts/run_gold.sh ]; then echo "scripts/run_gold.sh is added in Task 15" >&2; exit 1; fi
	env ANVIL_LANG=$(ANVIL_LANG) DATASET=$(DATASET) ANVIL_PRESET=$(ANVIL_PRESET) bash scripts/run_gold.sh

# ============================================================================
# Run on hardware / 在硬件上运行
# ============================================================================

# run-host — Execute host binary on real hardware / 在真实硬件上执行主机程序
run-host: build gen
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
	cmake --preset $(ANVIL_PRESET)
	cmake --build --preset $(ANVIL_PRESET) --target saxpy_xclbin
	@if [ -n "$(ANVIL_SYSROOT)" ]; then \
		env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET); \
	else \
		cmake --preset $(ANVIL_HOST_PRESET); \
	fi
	cmake --build --preset $(ANVIL_HOST_PRESET) --target run_saxpy
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	echo ""
	echo "Embedded hw_emu requires QEMU — see docs/deploy.md#qemu-emulation"
	echo "  xclbin : $(XCLBIN_PATH)"
	echo "  host   : $(HOST_BIN)  (AArch64)"
	echo "  emcfg  : $(HWEMU_BUILD_DIR)/emconfig.json"
else
xrt-emu: gen
	@if [ "$(ANVIL_DEVICE_KIND)" != "accelerator" ]; then echo "make xrt-emu currently requires an accelerator target with a combined host+kernel hw_emu preset; got TARGET=$(TARGET) ($(ANVIL_DEVICE_KIND))" >&2; exit 1; fi
	cmake --preset $(ANVIL_HWEMU_PRESET)
	cmake --build --preset $(ANVIL_HWEMU_PRESET) --target run_saxpy saxpy_xclbin
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) BUILD_DIR=$(HWEMU_BUILD_DIR) bash scripts/emconfig.sh
	@if [ ! -x $(HWEMU_HOST_BIN) ]; then echo "$(HWEMU_HOST_BIN) not built; use a hw_emu preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(HWEMU_XCLBIN_PATH) ]; then echo "$(HWEMU_XCLBIN_PATH) not found; make xrt-emu should have built saxpy_xclbin" >&2; exit 1; fi
	env XCL_EMULATION_MODE=hw_emu EMCONFIG_PATH=$(HWEMU_BUILD_DIR) $(HWEMU_HOST_BIN) --xclbin $(HWEMU_XCLBIN_PATH) --data-dir data/$(DATASET) --output data/$(DATASET)/xrt_emu_out.bin
endif

# xrt-hw — Run on real hardware (alias for run-host) / 在真实硬件上运行（run-host 的别名）
xrt-hw: build
	$(MAKE) run-host

# ============================================================================
# Analysis and comparison / 分析与对比
# ============================================================================

# compare — Compare output against golden reference / 将输出与黄金参考对比
compare:
	@if [ ! -f scripts/compare.py ]; then echo "scripts/compare.py is added in Task 16" >&2; exit 1; fi
	$(MAKE) build-python
	$(PYTHON) scripts/compare.py --dataset $(DATASET)

# analyze — Parse HLS synthesis reports / 解析 HLS 综合报告
analyze:
	@if [ ! -f scripts/analyze.py ]; then echo "scripts/analyze.py is added in Task 16" >&2; exit 1; fi
	$(MAKE) build-python
	$(PYTHON) scripts/analyze.py --build-dir $(BUILD_DIR)

# analyze-flow — Deep HLS flow analysis via hlsflow / 通过 hlsflow 进行深度 HLS 流程分析
analyze-flow:
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	$(MAKE) build-python
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(KERNEL)

# check-hls — Run HLS threshold checks / 运行 HLS 阈值检查
check-hls:
	$(MAKE) build-python
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
test-csynth: configure
	ctest --test-dir $(BUILD_DIR) -L csynth -V

test-cosim:
	@if [ -z "$(strip $(ANVIL_COSIM_TARGETS))" ]; then echo "cosim is not configured for TARGET=$(TARGET)" >&2; exit 1; fi
	$(MAKE) configure TARGET=$(TARGET)
	ctest --test-dir $(BUILD_DIR) -L cosim -V

test-xrt-emu:
	$(MAKE) xrt-emu

# test-xrt-hw — Full end-to-end hardware test / 完整的端到端硬件测试
# Steps / 步骤: build (cross-compile + xclbin) → gen → deploy+run via board_run.py → compare
# Requires / 需要: BOARD_IP and (for zcu102) PETALINUX_SYSROOT + Vitis env
test-xrt-hw:
	@if [ -z "$(BOARD_IP)" ]; then echo "ERROR: BOARD_IP not set. Usage: make test-xrt-hw BOARD_IP=<ip> [TARGET=zcu102] [DATASET=tiny]" >&2; exit 1; fi
	$(MAKE) build TARGET=$(TARGET)
	$(MAKE) xclbin TARGET=$(TARGET)
	$(MAKE) gen DATASET=$(DATASET)
	$(MAKE) build-python
	$(PYTHON) scripts/board_run.py \
		--board-ip "$(BOARD_IP)" \
		--ssh-user "$(BOARD_SSH_USER)" \
		--deploy-dir "$(BOARD_DEPLOY_DIR)" \
		--xclbin "$(XCLBIN_PATH)" \
		--host-bin "$(HOST_BIN)" \
		--dataset "$(DATASET)"
	$(MAKE) compare DATASET=$(DATASET)

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
	@if [ ! -f "$(HOST_BIN)" ]; then echo "ERROR: $(HOST_BIN) not found. Run: make build TARGET=$(TARGET) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	scp "$(HOST_BIN)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/run_saxpy"

# deploy-xclbin — Deploy the .xclbin bitstream / 部署 .xclbin 比特流
deploy-xclbin: deploy-check
	@if [ ! -f "$(XCLBIN_PATH)" ]; then echo "ERROR: $(XCLBIN_PATH) not found. Run: make xclbin TARGET=$(TARGET) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	scp "$(XCLBIN_PATH)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/saxpy.xclbin"

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

help:
	@echo "Targets:"
	@echo "  make build [TARGET=u250|u55c|zcu104|zcu102]  — configure + build C++ + pip install"
	@echo "  make test                         — CPU-only fast tests (no Vitis/XRT)"
	@echo "  make csynth                       — v++ HLS synthesis"
	@echo "  make cosim                        — HLS co-simulation"
	@echo "  make xclbin                       — link .xclbin"
	@echo "  make csynth-stream/cosim-stream   — opt-in U250 k2k stream kernel checks"
	@echo "  make pipeline-demo                — link U250 saxpy_stream→vadd_stream xclbin"
	@echo "  make gen                          — generate dataset"
	@echo "  make gold [ANVIL_LANG=cpp|python]       — run gold reference"
	@echo "  make xrt-emu                      — run accelerator hw_emu or build embedded + print QEMU note"
	@echo "  make xrt-hw                       — run on real hardware"
	@echo "  make compare                      — compare outputs vs gold"
	@echo "  make analyze                      — parse HLS csynth report"
	@echo "  make analyze-flow [KERNEL=all]    — hlsflow collect csynth (rich + HTML + JSONL)"
	@echo "  make check-hls                    — hlsflow threshold check on latest run"
	@echo "  make compare-hls BASELINE=<id> CANDIDATE=<id> — diff two runs"
	@echo "  make emconfig                     — generate emconfig.json for hw_emu"
	@echo "  make test-csynth/cosim/xrt-emu    — label-specific hardware tests"
	@echo "  make clean [TARGET=...]           — remove preset build dir"
	@echo "  make clean-all                    — remove all build dirs + eggs"
	@echo "  make deploy [TARGET=zcu102] BOARD_IP=<ip>     — scp bin+xclbin+data to board"
	@echo "  make deploy-bin/xclbin/data BOARD_IP=<ip>     — deploy individual artifact"
	@echo "  make test-xrt-hw BOARD_IP=<ip> [DATASET=tiny] — build+xclbin+gen+deploy+run+compare on board"
