# Get started: run the full flow

This page walks through the complete Vitis-Anvil flow from clone to CPU tests, HLS synthesis, cosim, xclbin link, and hardware execution.

## 0. Prerequisites

Minimum for CPU-only work:

- CMake and Ninja
- C++ compiler
- Python 3.10+
- `make`

Needed for Vitis/XRT work:

- Vitis 2024.2 or compatible Vitis installation
- XRT for accelerator-card host builds and hardware runs
- A matching platform `.xpfm`
- For embedded host builds: a PetaLinux sysroot

Source Vitis before FPGA flows:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

Create the Python environment explicitly:

```bash
make python-env
```

The Makefile prefers `uv` when available and falls back to `python3 -m venv`. It uses the USTC PyPI mirror by default; disable the mirror with:

```bash
make python-env PYPI_INDEX=
```

## 1. CPU-only sanity check

This is the Day-0 test. It does not use Vitis, XRT, a platform `.xpfm`, or FPGA hardware.

```bash
make test
```

It runs C++ unit/smoke tests and Python tests. Start here after every clone or refactor.

## 2. Pick a target

Common targets:

| `TARGET=` | Kind | Notes |
|---|---|---|
| `u250` | accelerator card | first-class path in this repo |
| `u50` | accelerator card | requires your installed U50 `.xpfm` path |
| `u55c` | accelerator card | HBM-oriented config |
| `u200`, `u280`, `vck5000` | accelerator card | config scaffolds; verify local platform package |
| `zcu102`, `zcu104` | embedded | first-class embedded paths |
| `zcu106`, `kv260` | embedded | config scaffolds; verify platform/sysroot |

Find installed platforms:

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

Override a platform path when your installation differs from the default:

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

## 3. Build host code

Build the selected host app without synthesizing kernels:

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

For embedded host cross-compile:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

## 4. Generate data and gold output

```bash
make gen DATASET=tiny
make gold DATASET=tiny ANVIL_LANG=cpp
make gold DATASET=tiny ANVIL_LANG=python
```

Datasets live under `data/<dataset>/`. The default demo writes binary float buffers plus metadata.

## 5. HLS synthesis

Use `KERNEL=` to choose the kernel-level build target:

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=vadd
make csynth TARGET=u250 KERNEL=all
```

Then inspect reports:

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

`hlsflow` writes rich terminal output, `reports/<run_id>.html`, `reports/<run_id>.txt`, and appends to `reports/runs.jsonl`.

## 6. HLS cosim

Cosim runs kernel testbenches. It does not run XRT host programs.

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=vadd
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

The testbenches run two transactions so Vitis can report interval/throughput when available.

## 7. Link xclbin

Hardware link is intentionally explicit and long-running:

```bash
make xclbin TARGET=u250
```

For hardware emulation:

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 DATASET=tiny HOST_APP=run_saxpy
```

## 8. Run on hardware

Accelerator card:

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Embedded board:

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## 9. Command selection rules

| Variable | Use it for |
|---|---|
| `TARGET=` | board/platform selection |
| `KERNEL=` | HLS synthesis, cosim, and HLS report analysis |
| `HOST_APP=` | XRT host program selection |
| `DATASET=` | dataset size/name |
| `ANVIL_PLATFORM=` | override `.xpfm` path |
| `PETALINUX_SYSROOT=` | embedded host cross-compile sysroot |

Keep this split: `KERNEL` is for HLS; `HOST_APP` is for XRT runtime.
