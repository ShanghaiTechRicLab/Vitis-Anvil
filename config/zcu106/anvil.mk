# config/zcu106/anvil.mk
ANVIL_DEVICE_KIND  := embedded
ANVIL_VITIS_PART   := xczu7ev-ffvc1156-2-e
ANVIL_PLATFORM     := $(XILINX_VITIS)/base_platforms/xilinx_zcu106_base_202420_1/xilinx_zcu106_base_202420_1.xpfm
ANVIL_PRESET       := zcu106-kernel
ANVIL_HWEMU_PRESET := zcu106-kernel
ANVIL_HOST_PRESET  := zcu106-host
ANVIL_NEEDS_CROSS  := yes
ANVIL_SYSROOT      := $(PETALINUX_SYSROOT)
ANVIL_XCLBIN_MODE  := hw_emu
ANVIL_COSIM_TARGETS ?= saxpy_cosim vadd_cosim