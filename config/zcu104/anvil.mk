# config/zcu104/anvil.mk
ANVIL_DEVICE_KIND := embedded
ANVIL_VITIS_PART  := xczu7ev-ffvc1156-2-e
ANVIL_PLATFORM    := $(XILINX_VITIS)/base_platforms/xilinx_zcu104_base_202420_1/xilinx_zcu104_base_202420_1.xpfm
ANVIL_PRESET      := zcu104-kernel
ANVIL_HWEMU_PRESET := zcu104-kernel
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT     := $(PETALINUX_SYSROOT)
ANVIL_XCLBIN_MODE := hw_emu
ANVIL_HOST_PRESET  := zcu104-host
