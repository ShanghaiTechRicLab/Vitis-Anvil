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
# Component filter for HLS primitive component kernels / HLS 原语组件内核筛选
COMPONENT ?=
# XRT host application to build / 要构建的 XRT 主机程序
HOST_APP ?= run_saxpy
# Execution mode for xclbin/run stages: hw, hw_emu, or sw_emu.
MODE ?= hw
RUN_KEY ?= latest

# Include board-specific configuration / 引入板卡专用配置
include config/$(TARGET)/anvil.mk
# Optional declarative target schema used by the build-system redesign.
-include config/$(TARGET)/target.mk

# Infer xclbin name from the selected host app / 根据选中的 host app 推导 xclbin 名称
ifeq ($(HOST_APP),run_pipeline_demo)
XCLBIN_NAME := pipeline_demo
else
XCLBIN_NAME := saxpy
endif
# Whether to run `make compare` after test-hw / test-hw 后是否执行 `make compare`
# vadd and pipeline_demo produce nondeterministic output; skip compare by default
# vadd 和 pipeline_demo 输出不确定，默认跳过对比
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
# 从 config/<TARGET>/anvil.mk 提取的 CMake 命令行覆写；
# 允许用户在 make 命令行直接覆盖 ANVIL_PLATFORM。
CMAKE_PLATFORM_ARGS := -DANVIL_VITIS_PLATFORM=$(ANVIL_PLATFORM) -DANVIL_VITIS_PART=$(ANVIL_VITIS_PART)
# Build directory for the selected preset / 当前 preset 的构建目录
BUILD_DIR   := build/$(ANVIL_PRESET)
# Optional K2K stream pipeline config / 可选 K2K stream pipeline 配置
PIPELINE_DEMO_CFG := config/$(TARGET)/pipeline_demo.cfg
# Empty string if config file does not exist / 配置文件不存在时为空字符串
PIPELINE_DEMO_SUPPORTED := $(wildcard $(PIPELINE_DEMO_CFG))
# CMake targets for HLS synthesis / HLS 综合的 CMake target
ANVIL_KERNEL_TARGETS ?= saxpy_xo
# Resolve actual kernel targets based on KERNEL variable / 根据 KERNEL 变量确定实际构建目标
#   KERNEL=all           → all configured kernels / 所有已配置的内核
#   KERNEL=pipeline_demo → streaming kernels / 流式内核对
#   KERNEL=<name>        → single kernel / 单个内核
ifeq ($(KERNEL),all)
SELECTED_KERNEL_TARGETS := $(ANVIL_KERNEL_TARGETS)
else ifeq ($(KERNEL),pipeline_demo)
SELECTED_KERNEL_TARGETS := saxpy_stream_xo vadd_stream_xo
else
SELECTED_KERNEL_TARGETS := $(KERNEL)_xo
endif
ifeq ($(strip $(COMPONENT)),)
SELECTED_COMPONENT_KERNEL :=
else
SELECTED_COMPONENT_KERNEL := component_$(COMPONENT)
endif
# CMake targets for co-simulation / 协同仿真的 CMake target
ANVIL_COSIM_TARGETS ?= saxpy_cosim vadd_cosim
# Same selection logic as kernel targets for cosim / 同 kernel 目标一致的筛选逻辑
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
# Software emulation uses the hw_emu preset shape but a separate build tree and
# ANVIL_VITIS_TARGET=sw_emu, because sw_emu and hw_emu xclbins are not
# interchangeable.
ANVIL_SWEMU_PRESET ?= $(ANVIL_HWEMU_PRESET)
SWEMU_BUILD_DIR    := build/$(TARGET)-host-swemu
SWEMU_HOST_BIN     := $(SWEMU_BUILD_DIR)/src/host/$(HOST_APP)
SWEMU_XCLBIN_PATH  := $(SWEMU_BUILD_DIR)/src/kernels/$(XCLBIN_NAME)_xclbin/$(XCLBIN_NAME).xclbin
EMCONFIG_DIR       := build/$(TARGET)/emconfig
EMCONFIG_JSON      := $(EMCONFIG_DIR)/emconfig.json
RUN_DIR            := runs/$(TARGET)/$(MODE)/$(HOST_APP)/$(DATASET)/$(RUN_KEY)
HW_RUN_DIR         := runs/$(TARGET)/hw/$(HOST_APP)/$(DATASET)/$(RUN_KEY)
HWEMU_RUN_DIR      := runs/$(TARGET)/hw_emu/$(HOST_APP)/$(DATASET)/$(RUN_KEY)
SWEMU_RUN_DIR      := runs/$(TARGET)/sw_emu/$(HOST_APP)/$(DATASET)/$(RUN_KEY)
QEMU_RUN_DIR       := runs/$(TARGET)/qemu/$(HOST_APP)/$(DATASET)/$(RUN_KEY)
RUN_HW_OUTPUT     ?= $(HW_RUN_DIR)/out.bin
RUN_EMU_OUTPUT    ?= $(HWEMU_RUN_DIR)/out.bin
QEMU_OUTPUT       ?= $(QEMU_RUN_DIR)/out.bin
QEMU_LAUNCHER     ?=
QEMU_ARGS         ?=
DATASET_DIR       := data/$(DATASET)
DATASET_STAMP     := $(DATASET_DIR)/.gen.stamp
GOLD_STAMP        := $(DATASET_DIR)/.gold.$(ANVIL_LANG).stamp
CPP_GOLD_DEPS     := src/gold/cpp/saxpy_gold_main.cpp src/gold/cpp/saxpy_gold.cpp src/gold/include/gold/saxpy_gold.hpp
PY_GOLD_DEPS      := src/gold/python/saxpy_gold.py
# Python venv / Python 虚拟环境
PYTHON      := .venv/bin/python
PIP         := $(PYTHON) -m pip
# Stamp file to skip reinstall when pyproject.toml is unchanged / 缓存标记，避免重复安装
VENV_STAMP  := .venv/.anvil-install.stamp
# PyPI mirror; set empty to use default index / PyPI 镜像源；设为空则使用默认索引
PYPI_INDEX  ?= https://mirrors.ustc.edu.cn/pypi/simple
PIP_INDEX_ARGS := $(if $(PYPI_INDEX),-i $(PYPI_INDEX),)
# HLS flow Python entry / HLS 流程 Python 入口
HLSFLOW_PYTHON := PYTHONPATH=tools $(PYTHON)
# Deployment to physical board / 部署到物理板卡
BOARD_IP          ?=
BOARD_SSH_USER    ?= root
BOARD_DEPLOY_DIR  ?= ~/anvil-deploy

# --- Phony targets declaration / 伪目标声明 ---
.PHONY: all configure build build-python python-env rebuild-python require-python-env clean clean-all help help-en help-zh \
        configure-kernel configure-host build-host build-kernel build-all \
        csynth cosim xclbin \
        require-pipeline-demo csynth-stream cosim-stream pipeline-demo \
        gen gold swemu hwemu hw qemu compare analyze analyze-cosim analyze-link check-hls compare-hls emconfig \
        deploy deploy-bin deploy-xclbin deploy-data deploy-check \
        test test-csynth test-cosim test-hw test-slow test-all

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

build-host: configure-host
	cmake --build --preset $(ANVIL_HOST_PRESET) --target $(HOST_APP)

# build-kernel — Build HLS kernel(s) without triggering host build / 构建 HLS 内核，不触发 host 构建
build-kernel: configure-kernel
	cmake --build --preset $(ANVIL_PRESET) --target $(SELECTED_KERNEL_TARGETS)

# Python environment is explicit: normal `make build` does not create/update it.
# Defaults to USTC PyPI mirror; disable with `make python-env PYPI_INDEX=`.
# Python 环境需显式创建：`make build` 不会自动创建/更新虚拟环境。
# 默认使用 USTC PyPI 镜像；通过 `make python-env PYPI_INDEX=` 禁用。
build-python: python-env

# python-env — Create or update .venv (stamp-based, skips if up-to-date)
#              创建或更新 .venv（基于 stamp 文件，已最新则跳过）
python-env: $(VENV_STAMP)

# Stamp depends on pyproject.toml — change triggers reinstall
# Stamp 依赖于 pyproject.toml — 修改后自动触发重新安装
$(VENV_STAMP): pyproject.toml
	@# Prefer uv (much faster), fall back to venv+pip / 优先使用 uv（速度更快），回退到 venv+pip
	@if command -v uv >/dev/null 2>&1; then \
		uv venv .venv; \
		uv pip install $(PIP_INDEX_ARGS) -e ".[test]" --python $(PYTHON); \
	else \
		test -x $(PYTHON) || python3 -m venv .venv; \
		$(PIP) install $(PIP_INDEX_ARGS) -e ".[test]" --quiet; \
	fi
	@touch $(VENV_STAMP)

# rebuild-python — Force-recreate .venv from scratch / 强制从头重建 .venv
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

# require-python-env — Guard target: fail early if .venv is missing
#                      守卫目标：虚拟环境缺失时提前报错
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

.PHONY: csynth-component
csynth-component:
	@if [ -z "$(strip $(COMPONENT))" ]; then echo "COMPONENT is required" >&2; exit 1; fi
	$(MAKE) csynth TARGET=$(TARGET) KERNEL=$(SELECTED_COMPONENT_KERNEL)

# cosim — Run HLS co-simulation / 运行 HLS 协同仿真
cosim:
	@# Guard: pipeline_demo kernel requires the pipeline config file / pipeline_demo 内核需要对应的配置文件
	@if [ "$(KERNEL)" = "pipeline_demo" ] && [ -z "$(PIPELINE_DEMO_SUPPORTED)" ]; then echo "KERNEL=pipeline_demo requires $(PIPELINE_DEMO_CFG)" >&2; exit 1; fi
	@if [ -z "$(strip $(SELECTED_COSIM_TARGETS))" ]; then echo "cosim is not configured for TARGET=$(TARGET)" >&2; exit 1; fi
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target $(SELECTED_COSIM_TARGETS)

.PHONY: cosim-component
cosim-component:
	@if [ -z "$(strip $(COMPONENT))" ]; then echo "COMPONENT is required" >&2; exit 1; fi
	$(MAKE) cosim TARGET=$(TARGET) KERNEL=$(SELECTED_COMPONENT_KERNEL)

# xclbin — Link kernel into .xclbin bitstream / 将内核链接为 .xclbin 比特流
xclbin: configure-kernel
	cmake --build $(BUILD_DIR) --target $(XCLBIN_NAME)_xclbin

# --------------------------------------------------------------------------
# Streaming pipeline targets (require config/<TARGET>/pipeline_demo.cfg)
# 流式 pipeline 目标（需要 config/<TARGET>/pipeline_demo.cfg）
# --------------------------------------------------------------------------
require-pipeline-demo:
	@if [ -z "$(PIPELINE_DEMO_SUPPORTED)" ]; then \
		echo "error: stream pipeline requires $(PIPELINE_DEMO_CFG) (TARGET=$(TARGET))" >&2; exit 1; \
	fi

csynth-stream:
	$(MAKE) require-pipeline-demo TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_xo vadd_stream_xo

cosim-stream:
	$(MAKE) require-pipeline-demo TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target saxpy_stream_cosim vadd_stream_cosim

pipeline-demo:
	$(MAKE) require-pipeline-demo TARGET=$(TARGET)
	$(MAKE) configure-kernel TARGET=$(TARGET)
	cmake --build $(BUILD_DIR) --target pipeline_demo_xclbin

# ============================================================================
# Data generation and golden reference / 数据生成与黄金参考
# ============================================================================

# gen — Generate dataset / 生成数据集
gen: $(DATASET_STAMP)

$(DATASET_STAMP): scripts/gen_dataset.py | build-python
	@if [ ! -f scripts/gen_dataset.py ]; then echo "scripts/gen_dataset.py is added in Task 15" >&2; exit 1; fi
	@if [ -f "$(DATASET_DIR)/meta.json" ] && [ -f "$(DATASET_DIR)/x.bin" ] && [ -f "$(DATASET_DIR)/y.bin" ] && \
	      [ "$(DATASET_DIR)/meta.json" -nt scripts/gen_dataset.py ] && \
	      [ "$(DATASET_DIR)/x.bin" -nt scripts/gen_dataset.py ] && \
	      [ "$(DATASET_DIR)/y.bin" -nt scripts/gen_dataset.py ]; then \
		echo "data/$(DATASET) is up to date"; \
	else \
		$(PYTHON) scripts/gen_dataset.py --dataset $(DATASET); \
	fi
	@touch "$(DATASET_STAMP)"

# gold — Run golden reference to produce expected output / 运行黄金参考生成期望输出
gold: $(GOLD_STAMP)

$(GOLD_STAMP): $(DATASET_STAMP) scripts/run_gold.sh $(if $(filter cpp,$(ANVIL_LANG)),$(CPP_GOLD_DEPS),$(PY_GOLD_DEPS))
	@if [ ! -f scripts/run_gold.sh ]; then echo "scripts/run_gold.sh is added in Task 15" >&2; exit 1; fi
	@# Build C++ gold binary first if gold ref is in C++ / 如果黄金参考使用 C++，先构建 gold 二进制
	@if [ "$(ANVIL_LANG)" = "cpp" ]; then \
		$(MAKE) configure-host TARGET=$(TARGET); \
		cmake --build --preset $(ANVIL_HOST_PRESET) --target saxpy_gold_bin; \
	fi
	env ANVIL_LANG=$(ANVIL_LANG) DATASET=$(DATASET) ANVIL_PRESET=$(ANVIL_HOST_PRESET) bash scripts/run_gold.sh
	@touch "$(GOLD_STAMP)"

# ============================================================================
# Run targets / 运行目标
# ============================================================================

# hw — Execute host binary on real hardware / 在真实硬件上执行主机程序
hw: $(DATASET_STAMP)
	@# Guard: pipeline demo needs the streaming kernel pipeline config / pipeline demo 需要流式内核配置
	@if [ "$(HOST_APP)" = "run_pipeline_demo" ] && [ -z "$(PIPELINE_DEMO_SUPPORTED)" ]; then echo "HOST_APP=run_pipeline_demo requires $(PIPELINE_DEMO_CFG)" >&2; exit 1; fi
	$(MAKE) build-host TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	@if [ ! -x $(HOST_BIN) ]; then echo "$(HOST_BIN) not built; use a preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(XCLBIN_PATH) ]; then echo "$(XCLBIN_PATH) not found; run make xclbin first" >&2; exit 1; fi
	mkdir -p "$(HW_RUN_DIR)"
	$(HOST_BIN) --xclbin $(XCLBIN_PATH) --data-dir data/$(DATASET) --output "$(HW_RUN_DIR)/out.bin" >"$(HW_RUN_DIR)/stdout.log" 2>&1
	printf '{"target":"%s","mode":"hw","host_app":"%s","dataset":"%s","output":"%s"}\n' "$(TARGET)" "$(HOST_APP)" "$(DATASET)" "$(HW_RUN_DIR)/out.bin" >"$(HW_RUN_DIR)/run.json"

# emconfig — Generate shared emconfig.json for sw_emu/hw_emu / 生成 sw_emu/hw_emu 共享 emconfig.json
emconfig:
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) TARGET=$(TARGET) EMCONFIG_DIR=$(EMCONFIG_DIR) bash scripts/emconfig.sh

swemu: $(DATASET_STAMP)
	@if [ "$(ANVIL_DEVICE_KIND)" != "accelerator" ]; then echo "make swemu currently requires an accelerator target; got TARGET=$(TARGET) ($(ANVIL_DEVICE_KIND))" >&2; exit 1; fi
	$(MAKE) emconfig TARGET=$(TARGET)
	cmake --preset $(ANVIL_SWEMU_PRESET) -B $(SWEMU_BUILD_DIR) $(CMAKE_PLATFORM_ARGS) -DANVIL_VITIS_TARGET=sw_emu
	cmake --build $(SWEMU_BUILD_DIR) --target $(HOST_APP) $(XCLBIN_NAME)_xclbin
	@if [ ! -x $(SWEMU_HOST_BIN) ]; then echo "$(SWEMU_HOST_BIN) not built; use a preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(SWEMU_XCLBIN_PATH) ]; then echo "$(SWEMU_XCLBIN_PATH) not found; make swemu should have built $(XCLBIN_NAME)_xclbin" >&2; exit 1; fi
	mkdir -p "$(SWEMU_RUN_DIR)"
	env XCL_EMULATION_MODE=sw_emu EMCONFIG_PATH=$(EMCONFIG_DIR) $(SWEMU_HOST_BIN) --xclbin $(SWEMU_XCLBIN_PATH) --data-dir data/$(DATASET) --output "$(SWEMU_RUN_DIR)/out.bin" >"$(SWEMU_RUN_DIR)/stdout.log" 2>&1
	printf '{"target":"%s","mode":"sw_emu","host_app":"%s","dataset":"%s","output":"%s"}\n' "$(TARGET)" "$(HOST_APP)" "$(DATASET)" "$(SWEMU_RUN_DIR)/out.bin" >"$(SWEMU_RUN_DIR)/run.json"

hwemu: $(DATASET_STAMP)
	@if [ "$(ANVIL_DEVICE_KIND)" != "accelerator" ]; then echo "make hwemu currently requires an accelerator target; use make qemu for embedded targets" >&2; exit 1; fi
	$(MAKE) emconfig TARGET=$(TARGET)
	cmake --preset $(ANVIL_HWEMU_PRESET) $(CMAKE_PLATFORM_ARGS)
	cmake --build --preset $(ANVIL_HWEMU_PRESET) --target $(HOST_APP) $(XCLBIN_NAME)_xclbin
	@if [ ! -x $(HWEMU_HOST_BIN) ]; then echo "$(HWEMU_HOST_BIN) not built; use a hw_emu preset with ANVIL_BUILD_XRT=ON" >&2; exit 1; fi
	@if [ ! -f $(HWEMU_XCLBIN_PATH) ]; then echo "$(HWEMU_XCLBIN_PATH) not found; make hwemu should have built $(XCLBIN_NAME)_xclbin" >&2; exit 1; fi
	mkdir -p "$(HWEMU_RUN_DIR)"
	env XCL_EMULATION_MODE=hw_emu EMCONFIG_PATH=$(EMCONFIG_DIR) $(HWEMU_HOST_BIN) --xclbin $(HWEMU_XCLBIN_PATH) --data-dir data/$(DATASET) --output "$(HWEMU_RUN_DIR)/out.bin" >"$(HWEMU_RUN_DIR)/stdout.log" 2>&1
	printf '{"target":"%s","mode":"hw_emu","host_app":"%s","dataset":"%s","output":"%s"}\n' "$(TARGET)" "$(HOST_APP)" "$(DATASET)" "$(HWEMU_RUN_DIR)/out.bin" >"$(HWEMU_RUN_DIR)/run.json"

qemu: $(DATASET_STAMP)
	@if [ "$(ANVIL_DEVICE_KIND)" != "embedded" ]; then echo "make qemu currently requires an embedded target; use make swemu/hwemu for accelerator targets" >&2; exit 2; fi
	$(MAKE) emconfig TARGET=$(TARGET)
	cmake --preset $(ANVIL_PRESET) $(CMAKE_PLATFORM_ARGS)
	cmake --build --preset $(ANVIL_PRESET) --target $(XCLBIN_NAME)_xclbin
	@if [ -n "$(ANVIL_SYSROOT)" ]; then \
		env SYSROOT="$(ANVIL_SYSROOT)" cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	else \
		cmake --preset $(ANVIL_HOST_PRESET) $(CMAKE_PLATFORM_ARGS); \
	fi
	cmake --build --preset $(ANVIL_HOST_PRESET) --target $(HOST_APP)
	env ANVIL_PLATFORM=$(ANVIL_PLATFORM) TARGET=$(TARGET) EMCONFIG_DIR=$(EMCONFIG_DIR) bash scripts/emconfig.sh
	@if [ -z "$(QEMU_LAUNCHER)" ]; then echo "make qemu requires QEMU_LAUNCHER=/path/to/launcher (or set it in config/$(TARGET)/target.mk)" >&2; exit 2; fi
	@if [ ! -x $(HOST_BIN) ]; then echo "$(HOST_BIN) not built" >&2; exit 1; fi
	@if [ ! -f $(XCLBIN_PATH) ]; then echo "$(XCLBIN_PATH) not found" >&2; exit 1; fi
	mkdir -p "$(QEMU_RUN_DIR)"
	env TARGET="$(TARGET)" HOST_APP="$(HOST_APP)" DATASET="$(DATASET)" RUN_KEY="$(RUN_KEY)" \
		ANVIL_PLATFORM="$(ANVIL_PLATFORM)" EMCONFIG_PATH="$(EMCONFIG_DIR)" \
		HOST_BIN="$(HOST_BIN)" XCLBIN_PATH="$(XCLBIN_PATH)" DATA_DIR="data/$(DATASET)" \
		RUN_DIR="$(QEMU_RUN_DIR)" OUTPUT="$(QEMU_OUTPUT)" \
		$(QEMU_LAUNCHER) $(QEMU_ARGS) >"$(QEMU_RUN_DIR)/stdout.log" 2>&1
	@if [ ! -f "$(QEMU_OUTPUT)" ]; then echo "QEMU launcher completed but did not create $(QEMU_OUTPUT)" >&2; exit 1; fi
	printf '{"target":"%s","mode":"qemu","host_app":"%s","dataset":"%s","output":"%s"}\n' "$(TARGET)" "$(HOST_APP)" "$(DATASET)" "$(QEMU_OUTPUT)" >"$(QEMU_RUN_DIR)/run.json"

# ============================================================================
# Analysis and comparison / 分析与对比
# ============================================================================

# compare — Compare output against golden reference / 将输出与黄金参考对比
compare:
	@if [ ! -f scripts/compare.py ]; then echo "scripts/compare.py is added in Task 16" >&2; exit 1; fi
	$(MAKE) build-python
	$(PYTHON) scripts/compare.py --dataset $(DATASET) --emu-output "$(RUN_EMU_OUTPUT)" --hw-output "$(RUN_HW_OUTPUT)"

# analyze — Deep HLS flow analysis via hlsflow / 通过 hlsflow 进行深度 HLS 流程分析
analyze: require-python-env
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	@if [ "$(BUILD)" = "1" ]; then $(MAKE) csynth TARGET=$(TARGET) KERNEL=$(KERNEL); fi
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(KERNEL) --target csynth --platform $(TARGET)

.PHONY: analyze-component
analyze-component:
	@if [ -z "$(strip $(COMPONENT))" ]; then echo "COMPONENT is required" >&2; exit 1; fi
	$(MAKE) analyze TARGET=$(TARGET) KERNEL=$(SELECTED_COMPONENT_KERNEL)

# analyze-cosim — Collect cosim report into hlsflow database / 收集 cosim 报告到 hlsflow 数据库
analyze-cosim: require-python-env
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	@if [ "$(BUILD)" = "1" ]; then $(MAKE) cosim TARGET=$(TARGET) KERNEL=$(KERNEL); fi
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(KERNEL) --target cosim --platform $(TARGET)

# analyze-link — Collect v++ link/xclbin report into hlsflow database / 收集 v++ link/xclbin 报告到 hlsflow 数据库
analyze-link: require-python-env
	@if [ ! -d tools/hlsflow ]; then echo "tools/hlsflow not present" >&2; exit 1; fi
	@if [ "$(BUILD)" = "1" ]; then $(MAKE) xclbin TARGET=$(TARGET) HOST_APP=$(HOST_APP); fi
	env $(HLSFLOW_PYTHON) -m hlsflow collect --build-dir $(BUILD_DIR) --kernel $(XCLBIN_NAME) --target link --platform $(TARGET)

# check-hls — Run HLS threshold checks / 运行 HLS 阈值检查
check-hls: require-python-env
	env $(HLSFLOW_PYTHON) -m hlsflow check --max-ii 1

# compare-hls — Diff two HLS runs / 对比两次 HLS 运行结果
compare-hls:
	@# Both BASELINE and CANDIDATE are required / 两个参数都必须提供
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
# Run CTest with csynth label / 使用 csynth 标签运行 CTest
test-csynth: configure-kernel
	ctest --test-dir $(BUILD_DIR) -L csynth -V

# test-cosim — Run cosim test (delegates to cosim target) / 运行 cosim 测试（委托给 cosim 目标）
test-cosim:
	$(MAKE) cosim TARGET=$(TARGET) KERNEL=$(KERNEL)

# test-hw — Full end-to-end hardware test / 完整的端到端硬件测试
# Steps / 步骤: build (cross-compile + xclbin) → dataset stamp → deploy+run via board_run.py → compare
# Requires / 需要: BOARD_IP and (for embedded targets) PETALINUX_SYSROOT + Vitis env
test-hw: $(DATASET_STAMP)
	@if [ -z "$(BOARD_IP)" ]; then echo "ERROR: BOARD_IP not set. Usage: make test-hw BOARD_IP=<ip> [TARGET=zcu102|kv260] [DATASET=tiny]" >&2; exit 1; fi
	$(MAKE) build-host TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	$(MAKE) xclbin TARGET=$(TARGET) HOST_APP=$(HOST_APP)
	$(MAKE) build-python
	$(PYTHON) scripts/board_run.py \
		--board-ip "$(BOARD_IP)" \
		--ssh-user "$(BOARD_SSH_USER)" \
		--deploy-dir "$(BOARD_DEPLOY_DIR)" \
		--xclbin "$(XCLBIN_PATH)" \
		--host-bin "$(HOST_BIN)" \
		--host-app "$(HOST_APP)" \
		--xclbin-name "$(XCLBIN_NAME)" \
		--dataset "$(DATASET)" \
		--local-output "$(HW_RUN_DIR)/out.bin"
	@# Only compare if the host app produces deterministic output / 仅当 host app 产生确定性输出时才进行对比
	@if [ "$(COMPARE_AFTER_HW)" = "yes" ]; then $(MAKE) compare DATASET=$(DATASET); else echo "Skipping make compare for HOST_APP=$(HOST_APP)"; fi

# test-slow — Run all slow hardware tests + slow Python tests / 运行所有慢速硬件测试和 Python 测试
test-slow: configure
	ctest --test-dir $(BUILD_DIR) -L "csynth|cosim|hwemu" -V
	@$(PYTHON) -m pytest -m slow tests/python -v; status=$$?; if [ $$status -eq 5 ]; then echo "No slow Python tests selected"; elif [ $$status -ne 0 ]; then exit $$status; fi

# test-all — Run all tests (fast + slow) / 运行所有测试（快速 + 慢速）
test-all: test test-slow

# ============================================================================
# Cleanup / 清理
# ============================================================================

# clean — Remove build directory for the current preset / 移除当前 preset 的构建目录
# Also removes the host build dir if it differs from kernel build dir
# 同时移除与 kernel 构建目录不同的 host 构建目录
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

# deploy-check — Guard: verify BOARD_IP is set before deploying / 守卫：部署前检查 BOARD_IP 是否设置
deploy-check:
	@if [ -z "$(BOARD_IP)" ]; then echo "ERROR: BOARD_IP not set. Usage: make deploy BOARD_IP=<ip> [BOARD_SSH_USER=root] [DATASET=tiny]" >&2; exit 1; fi

# deploy-bin — Deploy the host binary. Renamed to $(HOST_APP) on board.
#              部署主机端二进制文件，在板卡上重命名为 $(HOST_APP)
deploy-bin: deploy-check
	@if [ ! -f "$(HOST_BIN)" ]; then echo "ERROR: $(HOST_BIN) not found. Run: make build TARGET=$(TARGET) HOST_APP=$(HOST_APP) first." >&2; exit 1; fi
	ssh $(BOARD_SSH_USER)@$(BOARD_IP) "mkdir -p $(BOARD_DEPLOY_DIR)"
	scp "$(HOST_BIN)" "$(BOARD_SSH_USER)@$(BOARD_IP):$(BOARD_DEPLOY_DIR)/$(HOST_APP)"

# deploy-xclbin — Deploy the .xclbin bitstream. Renamed to $(XCLBIN_NAME).xclbin on board.
#                部署 .xclbin 比特流，在板卡上重命名为 $(XCLBIN_NAME).xclbin
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

# help — Default to English help / 默认显示英文帮助
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
	@echo "  make analyze TARGET=u250 KERNEL=saxpy          Analyze existing csynth reports"
	@echo "  make analyze-cosim TARGET=u250 KERNEL=saxpy    Analyze cosim reports"
	@echo "  make analyze-link TARGET=u250 HOST_APP=run_saxpy Analyze link/xclbin reports"
	@echo "  make csynth-component TARGET=u250 COMPONENT=tile_burst"
	@echo "  make csynth-component TARGET=u250 COMPONENT=radix_sort"
	@echo "  make csynth-component TARGET=u250 COMPONENT=prefix_histogram"
	@echo "  make csynth-component TARGET=u250 COMPONENT=counting_sort_buffer"
	@echo "  make cosim-component TARGET=u250 COMPONENT=pingpong_dbuf_lcs"
	@echo "  make cosim-component TARGET=u250 COMPONENT=line_window"
	@echo "  make cosim-component TARGET=u250 COMPONENT=prefix_histogram"
	@echo "  make cosim-component TARGET=u250 COMPONENT=counting_sort_buffer"
	@echo "  make analyze-component TARGET=u250 COMPONENT=tile_burst"
	@echo "  make check-hls                                 Check latest HLS run thresholds"
	@echo "  make compare-hls BASELINE=<id> CANDIDATE=<id>  Compare two HLS runs"
	@echo ""
	@echo "XRT / hardware flow:"
	@echo "  make gen DATASET=tiny                          Generate input dataset"
	@echo "  make gold DATASET=tiny ANVIL_LANG=cpp          Generate gold output"
	@echo "  make xclbin TARGET=u250                        Link hardware xclbin; long-running"
	@echo "  make swemu TARGET=u250 HOST_APP=run_saxpy      Run software emulation"
	@echo "  make hwemu TARGET=u250 HOST_APP=run_saxpy      Run accelerator hw_emu"
	@echo "  make qemu TARGET=zcu102 HOST_APP=run_saxpy     Run embedded QEMU via QEMU_LAUNCHER"
	@echo "  make hw TARGET=u250 HOST_APP=run_saxpy         Run selected host app on real hardware"
	@echo "  make compare DATASET=tiny                      Compare output with gold"
	@echo ""
	@echo "Embedded deployment flow:"
	@echo "  PETALINUX_SYSROOT=/path make build-host TARGET=zcu102 HOST_APP=run_saxpy"
	@echo "  make xclbin TARGET=zcu102"
	@echo "  make deploy TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make test-hw TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make deploy-bin|deploy-xclbin|deploy-data TARGET=zcu102 BOARD_IP=<ip>"
	@echo ""
	@echo "Stream pipeline demo:"
	@echo "  make csynth-stream TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make cosim-stream TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make pipeline-demo TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make hw TARGET=u250|u55c|u50|u200|u280|vck5000 HOST_APP=run_pipeline_demo"
	@echo ""
	@echo "Maintenance:"
	@echo "  make configure|configure-kernel|configure-host  Configure CMake presets"
	@echo "  make build-kernel|build-all                     Build kernel or complete local artifacts"
	@echo "  make analyze|analyze-cosim|analyze-link          Analysis entrypoints"
	@echo "  make test|test-csynth|test-cosim                HLS model and Vitis test entrypoints"
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
	@echo "  make analyze TARGET=u250 KERNEL=saxpy          分析已有 csynth 报告"
	@echo "  make analyze-cosim TARGET=u250 KERNEL=saxpy    分析 cosim 报告"
	@echo "  make analyze-link TARGET=u250 HOST_APP=run_saxpy 分析 link/xclbin 报告"
	@echo "  make csynth-component TARGET=u250 COMPONENT=tile_burst"
	@echo "  make csynth-component TARGET=u250 COMPONENT=radix_sort"
	@echo "  make csynth-component TARGET=u250 COMPONENT=prefix_histogram"
	@echo "  make csynth-component TARGET=u250 COMPONENT=counting_sort_buffer"
	@echo "  make cosim-component TARGET=u250 COMPONENT=pingpong_dbuf_lcs"
	@echo "  make cosim-component TARGET=u250 COMPONENT=line_window"
	@echo "  make cosim-component TARGET=u250 COMPONENT=prefix_histogram"
	@echo "  make cosim-component TARGET=u250 COMPONENT=counting_sort_buffer"
	@echo "  make analyze-component TARGET=u250 COMPONENT=tile_burst"
	@echo "  make check-hls                                 检查最新 HLS run 阈值"
	@echo "  make compare-hls BASELINE=<id> CANDIDATE=<id>  对比两次 HLS run"
	@echo ""
	@echo "XRT / 硬件流程:"
	@echo "  make gen DATASET=tiny                          生成输入数据"
	@echo "  make gold DATASET=tiny ANVIL_LANG=cpp          生成 gold output"
	@echo "  make xclbin TARGET=u250                        链接硬件 xclbin；耗时较长"
	@echo "  make swemu TARGET=u250 HOST_APP=run_saxpy      跑 software emulation"
	@echo "  make hwemu TARGET=u250 HOST_APP=run_saxpy      跑加速卡 hw_emu"
	@echo "  make qemu TARGET=zcu102 HOST_APP=run_saxpy     通过 QEMU_LAUNCHER 跑 embedded QEMU"
	@echo "  make hw TARGET=u250 HOST_APP=run_saxpy         在真实硬件上运行选定 host app"
	@echo "  make compare DATASET=tiny                      和 gold 比较输出"
	@echo ""
	@echo "Embedded 部署流程:"
	@echo "  PETALINUX_SYSROOT=/path make build-host TARGET=zcu102 HOST_APP=run_saxpy"
	@echo "  make xclbin TARGET=zcu102"
	@echo "  make deploy TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make test-hw TARGET=zcu102 BOARD_IP=<ip> DATASET=tiny"
	@echo "  make deploy-bin|deploy-xclbin|deploy-data TARGET=zcu102 BOARD_IP=<ip>"
	@echo ""
	@echo "Stream pipeline demo:"
	@echo "  make csynth-stream TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make cosim-stream TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make pipeline-demo TARGET=u250|u55c|u50|u200|u280|vck5000"
	@echo "  make hw TARGET=u250|u55c|u50|u200|u280|vck5000 HOST_APP=run_pipeline_demo"
	@echo ""
	@echo "维护:"
	@echo "  make configure|configure-kernel|configure-host  配置 CMake presets"
	@echo "  make build-kernel|build-all                     构建 kernel 或完整本地产物"
	@echo "  make analyze|analyze-cosim|analyze-link          分析入口"
	@echo "  make test|test-csynth|test-cosim                HLS 模型和 Vitis 测试入口"
	@echo "  make clean|clean-all                            清理构建产物"
	@echo "  make help-en                                    英文帮助"
	@echo ""
	@echo "文档: README.en.md, README.zh.md, docs/en/get_started.md, docs/zh/get_started.md"
