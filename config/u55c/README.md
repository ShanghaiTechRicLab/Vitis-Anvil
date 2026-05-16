# config/u55c/

Vitis build configuration for the Xilinx Alveo U55C (HBM2 board,
xcu55c-fsvh2892-2L-e).

## Variables

| Variable | Value |
|----------|-------|
| `ANVIL_VITIS_PART` | `xcu55c-fsvh2892-2L-e` |
| `ANVIL_PLATFORM` | `xilinx_u55c_gen3x16_xdma_3_202210_1` (confirm on your host; keep CMakePresets.json in sync) |
| `ANVIL_PRESET` | `u55c-host` |
| `ANVIL_XCLBIN_MODE` | `hw` |

## HBM bank mapping

`link.cfg` uses `sp=<kernel>_<inst>.<arg>:HBM[N]` (vs `DDR[N]` for U250).
The XRT host code is bank-agnostic: `kernel.MemGroupId(arg_index)` returns
the right group id derived from the xclbin connectivity at runtime — no
host changes needed when switching DDR↔HBM.

**Design conclusion:** `XrtBuffer` construction should work for HBM with no
host-side changes — `kernel.MemGroupId(arg_index)` resolves to the right HBM
group id at runtime. `XCL_BO_FLAGS_NONE` is the correct flag for DDR and HBM
Alveo cards.

## Notes

- U55C has 16 GiB HBM2 across 32 pseudo-channels (HBM[0..31]). Don't span
  more than one HBM stack per kernel argument unless you've measured.
- The Vitis 2024.2 base platform for U55C is `xilinx_u55c_gen3x16_xdma_3_202210_1`
  (or newer revisions; check `$XILINX_VITIS/base_platforms/` on your system).
