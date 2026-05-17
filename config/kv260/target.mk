# config/kv260/target.mk
# Stage-specific build metadata. This file contains platform facts and xclbin
# bundle schema; it intentionally does not contain CMake preset names.
TARGET_KIND := embedded
SUPPORTED_MODES := hw qemu
PLATFORM := $(XILINX_VITIS)/base_platforms/xilinx_kv260_base_202420_1/xilinx_kv260_base_202420_1.xpfm
PART := xck26-sfvc784-2lv-c
CLOCK_MHZ ?= 200
XRT_ROOT ?= /opt/xilinx/xrt
SYSROOT ?= $(PETALINUX_SYSROOT)
DEFAULT_XCLBIN := saxpy
KERNELS := saxpy vadd
HOST_APPS := run_saxpy run_vadd run_pipeline_demo
XCLBINS := saxpy pipeline_demo
XCLBIN_saxpy := saxpy_xo vadd_xo
XCLBIN_pipeline_demo := saxpy_stream_xo vadd_stream_xo
COSIM_TARGETS := saxpy_cosim vadd_cosim
# Set this to a board/BSP-provided executable script to make `make qemu` run.
# The launcher receives HOST_BIN, XCLBIN_PATH, DATA_DIR, RUN_DIR, OUTPUT,
# EMCONFIG_PATH, TARGET, HOST_APP, DATASET, and ANVIL_PLATFORM in its environment.
QEMU_LAUNCHER ?=
QEMU_ARGS ?=
