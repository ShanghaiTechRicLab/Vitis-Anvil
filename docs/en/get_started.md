# Get started: run the full flow

This page takes you from a fresh clone through CPU tests, HLS synthesis, cosimulation, xclbin linking, and hardware execution. Each step builds on the previous one, so you can stop at any point that matches what you need.

## 0. Prerequisites

Minimal setup for CPU-only work:

- CMake and Ninja
- A C++ compiler
- Python 3.10 or later
- `make`

For Vitis and XRT work you also need:

- Vitis 2024.2 (or a compatible version)
- XRT, for accelerator-card host builds and hardware runs
- A matching platform `.xpfm` file
- A PetaLinux sysroot if you are building for embedded boards

Before running any FPGA flow, source the Vitis environment:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

Create the Python environment explicitly — `make build` will not do it for you:

```bash
make python-env
```

The Makefile uses `uv` if it is installed, otherwise it falls back to `python3 -m venv`. It defaults to the USTC PyPI mirror; you can disable the mirror:

```bash
make python-env PYPI_INDEX=
```

## 1. CPU-only sanity check

This is the zero-day test. It does not use Vitis, XRT, a platform file, or FPGA hardware.

```bash
make test
```

It compiles and runs C++ unit tests and Python tests. Run this after every clone or after any refactoring — if this passes, your toolchain is set up correctly.

## 2. Pick a target

Which board are you targeting?

| `TARGET=` | Kind | Notes |
|---|---|---|
| `u250` | accelerator card | well-tested in this repository |
| `u50` | accelerator card | you need the U50 `.xpfm` installed |
| `u55c` | accelerator card | HBM-oriented configuration |
| `u200`, `u280`, `vck5000` | accelerator card | config scaffolds exist; verify your platform package |
| `zcu102`, `zcu104` | embedded | well-tested embedded targets |
| `zcu106`, `kv260` | embedded | config scaffolds exist; verify platform and sysroot |

To find which platform files you have installed:

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

If the default path in `config/<target>/anvil.mk` does not match your installation, override it:

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

## 3. Build the host code

Build a host app without synthesizing any kernels:

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

For an embedded target you need cross-compilation with a sysroot:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

## 4. Generate data and golden output

```bash
make gen DATASET=tiny
make gold DATASET=tiny ANVIL_LANG=cpp
make gold DATASET=tiny ANVIL_LANG=python
```

Datasets live under `data/<dataset>/`. The default demo writes binary float buffers plus metadata. The "gold" output is what you will later compare your hardware result against.

## 5. HLS synthesis

Use `KERNEL=` to choose which kernel to synthesize:

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=vadd
make csynth TARGET=u250 KERNEL=all
```

Once synthesis finishes, inspect the reports:

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

`hlsflow` writes a colored terminal report, exports `reports/<run_id>.html` and `reports/<run_id>.txt`, and appends the run to `reports/runs.jsonl` for later comparison.

## 6. HLS cosimulation

Cosimulation runs the kernel-level testbench against the RTL that Vitis generated. It does not run the XRT host program.

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=vadd
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

The testbenches execute two transactions per kernel, which gives Vitis enough data to report initiation interval and throughput where applicable.

## 7. Link the xclbin

Hardware linking takes time. It is an explicit separate step for that reason.

```bash
make xclbin TARGET=u250
```

For hardware emulation (no physical card needed):

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 DATASET=tiny HOST_APP=run_saxpy
```

## 8. Run on hardware

Accelerator card with a real xclbin:

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Embedded board (build, deploy, run remotely):

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## 9. How the variables work

| Variable | What it selects |
|---|---|
| `TARGET=` | Board or platform |
| `KERNEL=` | HLS kernel for synthesis, cosimulation, and report analysis |
| `HOST_APP=` | XRT host program to build and run |
| `DATASET=` | Dataset size or name under `data/` |
| `ANVIL_PLATFORM=` | Override the `.xpfm` path |
| `PETALINUX_SYSROOT=` | Sysroot for embedded cross-compilation |

`KERNEL` is for HLS work. `HOST_APP` is for XRT runtime work. They are separate concerns and you should keep them that way.
