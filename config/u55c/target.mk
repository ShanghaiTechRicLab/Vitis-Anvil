# config/u55c/target.mk
# Stage-specific build metadata. This file contains platform facts and xclbin
# bundle schema; it intentionally does not contain CMake preset names.
TARGET_KIND := accelerator
SUPPORTED_MODES := sw_emu hw_emu hw
PLATFORM := $(XILINX_VITIS)/base_platforms/xilinx_u55c_gen3x16_xdma_3_202210_1/xilinx_u55c_gen3x16_xdma_3_202210_1.xpfm
PART := xcu55c-fsvh2892-2L-e
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
