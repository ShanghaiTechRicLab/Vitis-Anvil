# src/kernels/

Vitis HLS kernel sources live here.

Phase 2 currently registers the `saxpy` HLS kernel:

- `saxpy_xo` runs Vitis HLS csynth and is included when kernel builds are enabled.
- `saxpy_cosim` is registered for U250 test builds.
- `saxpy_xclbin` is registered only for `ACCEL_PLATFORM_KIND=alveo_u250`; it is
  explicit, long-running, not part of `ALL`, and not added to ctest.

The xclbin link target defaults to `ACCEL_VITIS_TARGET` (`hw`, `hw_emu`, or
`sw_emu`).
