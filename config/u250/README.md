# Alveo U250 Vitis Link Configuration

This directory contains the Vitis linker configuration for the `saxpy.xclbin`
target on Alveo U250.

Phase 5 links two kernel instances into that xclbin:

- `saxpy_1`: `x`, `y`, and `out` mapped to DDR[0], DDR[1], DDR[2]
- `vadd_1`: `a`, `b`, and `out` mapped to DDR[0], DDR[1], DDR[2]

Both kernels request a 300 MHz kernel clock to match the default U250 preset.
The xclbin target is registered when `ANVIL_PLATFORM_KIND=u250`; embedded
platforms keep a saxpy-only xclbin while still exposing `vadd_xo` as a
buildable HLS target.
