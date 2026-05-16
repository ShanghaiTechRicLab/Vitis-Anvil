# config/vck5000/anvil.mk
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcvc1902-vsva2197-2MP-e-S
ANVIL_PLATFORM         := /opt/xilinx/platforms/xilinx_vck5000_gen4x8_qdma_2_202220_1/xilinx_vck5000_gen4x8_qdma_2_202220_1.xpfm
ANVIL_PRESET           := vck5000-host
ANVIL_HWEMU_PRESET     := vck5000-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS    :=
