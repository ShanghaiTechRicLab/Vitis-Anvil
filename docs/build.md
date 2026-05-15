# Build Guide

## Prerequisites

| Component | Required for |
|-----------|-------------|
| cmake ≥ 3.21, ninja | All builds |
| Python ≥ 3.10, pip | `make test`, `make compare`, scripts |
| Vitis 2023.x or 2024.x (`v++`, `vitis-run`) | `make csynth`, `make cosim`, `make xclbin` |
| U250 platform file (.xpfm) | `make build TARGET=u250`, `make csynth`, `make cosim`, `make xclbin` |
| XRT (`/opt/xilinx/xrt`) | `TARGET=u250` configure/build, `make xrt-emu`, `make xrt-hw` |
| PetaLinux sysroot (`SYSROOT`) | direct `zcu104-host` cross preset |

## Day-0 fast test (no Vitis, no FPGA)

```bash
rtk pip install -e .
make test
```

Runs `ctest --preset hls-model-linux-debug` (C++ unit tests + CLI smoke) and
`pytest -m fast tests/python`. It is CPU-only and does not touch Vitis, XRT, a
platform `.xpfm`, or FPGA hardware.

## CMake preset matrix

| Preset | `ANVIL_BUILD_KERNELS` | `ANVIL_BUILD_XRT` | Use |
|--------|-----------------------|-------------------|-----|
| `gold-linux-debug` | OFF | OFF | gold reference only |
| `hls-model-linux-debug` | OFF | OFF | CPU model + all unit tests |
| `u250-host` | ON | ON | U250 Vitis HLS/xclbin + native XRT host |
| `u250-host-hwemu` | ON | ON | U250 hw_emu xclbin + native XRT host |
| `zcu104-host` | OFF | ON | ZCU104 AArch64 XRT host cross-compile |
| `zcu104-kernel` | ON | OFF | ZCU104 kernel build preset (tests/cosim off) |

## ctest label matrix

| Label | `ctest --preset` runs? | Triggered by |
|-------|------------------------|-------------|
| (none) | ✓ | default fast CPU tests |
| `csynth` | ✗ | `make test-csynth` |
| `cosim` | ✗ | `make test-cosim` |
| `xrt_emu` | ✗ | reserved label; current `make test-xrt-emu` calls `make xrt-emu` directly |
| `build` | ✗ | internal HLS build fixture |

All CTest presets exclude `csynth|cosim|xrt_emu|build` labels by default.
Label-specific Makefile targets use `ctest --test-dir build/<preset>` to bypass
that preset filter intentionally.

## Selecting target platform

```bash
make build TARGET=u250     # Alveo U250; requires Vitis + U250 .xpfm + XRT
make build TARGET=zcu104   # ZCU104 kernel preset; requires Vitis + ZCU104 .xpfm
```

`config/<target>/anvil.mk` defines the platform variables consumed by the
Makefile. `TARGET=zcu104` selects the kernel preset; use the `zcu104-host` CMake preset directly for AArch64 host cross-compiles and set `SYSROOT` to your PetaLinux sysroot. Override `ANVIL_PLATFORM` or CMake cache values when your local tool installation differs from the lab defaults.

## Common workflows

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make xrt-emu TARGET=u250 DATASET=tiny
make compare DATASET=tiny

make csynth TARGET=u250
make cosim TARGET=u250
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 DATASET=tiny
```

`make xclbin` links a hardware `.xclbin` and is intentionally manual and
long-running; it is not part of the default fast test path.
