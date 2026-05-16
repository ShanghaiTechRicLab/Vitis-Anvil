# config/u55c/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu55c-fsvh2892-2L-e
# Site-specific: override ANVIL_PLATFORM if your U55C xpfm path differs.
# Run `platforminfo --list` to find the installed name on your system.
# configure will FATAL_ERROR if this path does not exist.
ANVIL_PLATFORM         := $(XILINX_VITIS)/base_platforms/xilinx_u55c_gen3x16_xdma_3_202210_1/xilinx_u55c_gen3x16_xdma_3_202210_1.xpfm
ANVIL_PRESET           := u55c-host
ANVIL_HWEMU_PRESET     := u55c-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
