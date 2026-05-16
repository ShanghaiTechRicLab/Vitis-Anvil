# Build reference

This page is the concise build reference. For the guided path, start with [Get started](get_started.md).

## Prerequisites

| Component | Required for |
|---|---|
| CMake, Ninja, C++ compiler | all local builds |
| Python 3.10+ | Python tools and tests |
| Vitis (`v++`, `vitis-run`) | HLS synthesis, cosim, xclbin |
| XRT | accelerator host builds and hardware runs |
| platform `.xpfm` | Vitis kernel/xclbin builds |
| PetaLinux sysroot | embedded AArch64 host builds |

## Fast local build

```bash
make python-env
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

`make build` builds the selected host binary only. It does not create the Python environment and does not run HLS synthesis.

## HLS build

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

## XRT build

```bash
make xclbin TARGET=u250
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## Target overrides

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50.xpfm
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

Variables: `TARGET` selects platform, `KERNEL` selects HLS kernel, `HOST_APP` selects XRT host app, `DATASET` selects data.
