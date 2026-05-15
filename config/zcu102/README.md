# config/zcu102/

Vitis build configuration for the Xilinx ZCU102 evaluation board
(Zynq UltraScale+ xczu9eg-ffvb1156-2-e).

## Variables

| Variable | Value |
|----------|-------|
| `ANVIL_VITIS_PART` | `xczu9eg-ffvb1156-2-e` |
| `ANVIL_PLATFORM` | `xilinx_zcu102_base_202420_1` (Vitis 2024.2) |
| `ANVIL_PRESET` | `zcu102-kernel` |
| `ANVIL_HOST_PRESET` | `zcu102-host` |
| `ANVIL_XCLBIN_MODE` | `hw_emu` |

## Notes

- `ANVIL_HOST_PRESET` differs from `ANVIL_PRESET`: kernel synthesis runs
  natively on x86; the XRT host binary is cross-compiled for AArch64.
- Set `PETALINUX_SYSROOT` to your PetaLinux sysroot before running
  `make build TARGET=zcu102`.
- See `platforms/zcu102/README.md` for board-side setup.
