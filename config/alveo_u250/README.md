# Alveo U250 Vitis Link Configuration

This directory contains the Vitis linker configuration for the Phase 2 `saxpy` xclbin target.

`link.cfg` creates one kernel instance named `saxpy_1`, maps the `x`, `y`, and `out` AXI ports to separate DDR banks, and requests a 300 MHz kernel clock to match the default U250 preset.

The xclbin is registered as the `saxpy_xclbin` target only when `ANVIL_PLATFORM_KIND=alveo_u250`; other kernel presets can still configure without this U250-specific link file.
