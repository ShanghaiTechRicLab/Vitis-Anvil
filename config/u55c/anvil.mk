# config/u55c/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu55c-fsvh2892-2L-e
# Site-specific: keep this in sync with CMakePresets.json ANVIL_VITIS_PLATFORM
# if your U55C xpfm path differs. Run `platforminfo --list` to find the
# installed name on your system; configure will FATAL_ERROR if absent.
ANVIL_PLATFORM         := $(XILINX_VITIS)/base_platforms/xilinx_u55c_gen3x16_xdma_3_202210_1/xilinx_u55c_gen3x16_xdma_3_202210_1.xpfm
ANVIL_PRESET           := u55c-host
ANVIL_HWEMU_PRESET     := u55c-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS     := saxpy_cosim vadd_cosim
