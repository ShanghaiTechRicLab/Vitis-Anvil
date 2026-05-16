# config/zcu102/anvil.mk
ANVIL_DEVICE_KIND  := embedded
ANVIL_VITIS_PART   := xczu9eg-ffvb1156-2-e
ANVIL_PLATFORM     := $(XILINX_VITIS)/base_platforms/xilinx_zcu102_base_202420_1/xilinx_zcu102_base_202420_1.xpfm
ANVIL_PRESET       := zcu102-kernel
ANVIL_HWEMU_PRESET := zcu102-kernel
ANVIL_HOST_PRESET  := zcu102-host
ANVIL_NEEDS_CROSS  := yes
ANVIL_SYSROOT      := $(PETALINUX_SYSROOT)
ANVIL_XCLBIN_MODE  := hw_emu

# Cosim testbenches are currently wired only for u250.
ANVIL_COSIM_TARGETS :=
