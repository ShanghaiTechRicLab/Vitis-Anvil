# config/u50/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu50-fsvh2104-2-e
ANVIL_PLATFORM         := /opt/xilinx/platforms/xilinx_u50_gen3x16_xdma_5_202210_1/xilinx_u50_gen3x16_xdma_5_202210_1.xpfm
ANVIL_PRESET           := u50-host
ANVIL_HWEMU_PRESET     := u50-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS ?= saxpy_cosim vadd_cosim
