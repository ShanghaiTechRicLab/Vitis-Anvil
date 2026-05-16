# config/kv260/anvil.mk
ANVIL_DEVICE_KIND  := embedded
ANVIL_VITIS_PART   := xck26-sfvc784-2lv-c
# KV260/K26 base platforms are not installed by default in every Vitis setup.
# Override ANVIL_PLATFORM or CMakePresets.json if your local platform name differs.
ANVIL_PLATFORM     := $(XILINX_VITIS)/base_platforms/xilinx_kv260_base_202420_1/xilinx_kv260_base_202420_1.xpfm
ANVIL_PRESET       := kv260-kernel
ANVIL_HWEMU_PRESET := kv260-kernel
ANVIL_HOST_PRESET  := kv260-host
ANVIL_NEEDS_CROSS  := yes
ANVIL_SYSROOT      := $(PETALINUX_SYSROOT)
ANVIL_XCLBIN_MODE  := hw_emu

ANVIL_COSIM_TARGETS ?= saxpy_cosim vadd_cosim