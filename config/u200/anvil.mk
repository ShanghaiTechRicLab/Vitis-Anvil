# config/u200/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu200-fsgd2104-2-e
ANVIL_PLATFORM         := /opt/xilinx/platforms/xilinx_u200_gen3x16_xdma_2_202110_1/xilinx_u200_gen3x16_xdma_2_202110_1.xpfm
ANVIL_PRESET           := u200-host
ANVIL_HWEMU_PRESET     := u200-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS    :=
