# src/kernels/

Vitis HLS kernel sources live here.

Phase 2 currently registers the `saxpy` HLS kernel:

- `saxpy_xo` runs Vitis HLS csynth and is included when kernel builds are enabled.
- `saxpy_cosim` is registered for U250 test builds.
- `saxpy_xclbin` is registered only for `ANVIL_PLATFORM_KIND=u250`; it is
  explicit, long-running, not part of `ALL`, and not added to ctest.

The xclbin link target defaults to `ANVIL_VITIS_TARGET` (`hw`, `hw_emu`, or
`sw_emu`).
