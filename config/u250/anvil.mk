# config/u250/anvil.mk
ANVIL_DEVICE_KIND := accelerator
ANVIL_VITIS_PART  := xcu250-figd2104-2L-e
ANVIL_PLATFORM    := /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
ANVIL_PRESET      := u250-host
ANVIL_NEEDS_CROSS := no
ANVIL_XCLBIN_MODE := hw
ANVIL_XRT_LIB     := /opt/xilinx/xrt
