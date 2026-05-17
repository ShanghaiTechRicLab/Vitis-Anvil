# config/vck5000/target.mk
# Stage-specific build metadata. This file contains platform facts and xclbin
# bundle schema; it intentionally does not contain CMake preset names.
TARGET_KIND := accelerator
SUPPORTED_MODES := sw_emu hw_emu hw
PLATFORM := /opt/xilinx/platforms/xilinx_vck5000_gen4x8_qdma_2_202220_1/xilinx_vck5000_gen4x8_qdma_2_202220_1.xpfm
PART := xcvc1902-vsva2197-2MP-e-S
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
