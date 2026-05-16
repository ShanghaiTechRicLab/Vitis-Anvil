# config/u280/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu280-fsvh2892-2L-e
ANVIL_PLATFORM         := /opt/xilinx/platforms/xilinx_u280_xdma_201920_3/xilinx_u280_xdma_201920_3.xpfm
ANVIL_PRESET           := u280-host
ANVIL_HWEMU_PRESET     := u280-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS    :=
