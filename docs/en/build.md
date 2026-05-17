# Build guide: what each command does

This page explains the Make targets. It is not just a list of commands; it explains what each target consumes and produces.

For the dependency model behind these commands, read [Build system model](build_system.md).

## 1. The four selectors

Most commands accept these variables:

```bash
make <target> TARGET=u250 KERNEL=saxpy HOST_APP=run_saxpy DATASET=tiny
```

| Variable | Selects | Example | If you choose wrong |
|---|---|---|---|
| `TARGET` | board/platform config | `u250`, `zcu102` | CMake uses wrong platform, part, sysroot, or link.cfg |
| `KERNEL` | HLS kernel target | `saxpy`, `vadd`, `all` | You synthesize/cosim the wrong kernel |
| `HOST_APP` | CPU executable | `run_saxpy` | You build/run the wrong host program |
| `DATASET` | input/reference dataset name | `tiny` | gold, run, compare look at different cases |

## 2. Fast CPU-only targets

### `make test`

Purpose: prove the CPU-side code is correct before using Vitis.

What it does:

1. configures a native debug build
2. builds tests and CPU utilities
3. runs CTest

What it produces:

- native binaries in `build/hls-model-linux-debug/`
- generated test data under that build tree

Use it when:

- after changing C++ logic
- after changing Python tools
- before running slow HLS commands

### `make build TARGET=<target> HOST_APP=<app>`

Purpose: build one host app quickly.

What it does:

1. reads `config/<target>/anvil.mk`
2. configures the host preset
3. builds `src/host/<HOST_APP>`

It does not synthesize kernels and does not build xclbin.

Example:

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

## 3. Python environment targets

Some analysis tools use Python. The normal `make build` target does not create a virtual environment.

```bash
make python-env
```

What it does:

- uses `uv` if available
- otherwise creates `.venv` with `python3 -m venv`
- installs the project test dependencies
- uses USTC PyPI mirror by default

Disable the mirror:

```bash
make python-env PYPI_INDEX=
```

Rebuild from scratch:

```bash
make rebuild-python
```

## 4. HLS synthesis targets

### `make csynth TARGET=<target> KERNEL=<kernel>`

Purpose: turn kernel C++ into Vitis HLS output and reports.

Example:

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

What it consumes:

- kernel sources in `src/kernels/`
- ABI headers in `src/kernels/include/kernels/`
- target config in `config/<target>/anvil.mk`
- Vitis platform path from that config

What it produces:

- HLS work directory
- `.xo` kernel object or packaged HLS output
- csynth XML and text reports

If this fails, inspect the Vitis HLS log first. Common causes:

- kernel code is not HLS-compatible C++
- missing include path
- invalid platform path
- unsupported C++ features for HLS

### `make analyze TARGET=<target> KERNEL=<kernel>`

Purpose: read HLS reports and show a human-friendly summary.

Example:

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

Use this after `csynth`. It does not synthesize by itself if reports already exist.

## 5. HLS cosim targets

### `make cosim TARGET=<target> KERNEL=<kernel>`

Purpose: run C/RTL cosimulation for one or more kernels.

Example:

```bash
make cosim TARGET=u250 KERNEL=vadd
```

What it consumes:

- kernel source
- cosim testbench from `tests/kernels/`
- Vitis HLS generated RTL

What it produces:

- cosim reports under the HLS work directory
- pass/fail status
- latency information if available

Cosim is a kernel test. It does not run a host app and does not load xclbin.

### `make analyze-cosim TARGET=<target> KERNEL=<kernel>`

Purpose: summarize cosim reports.

Example:

```bash
make analyze-cosim TARGET=zcu102 KERNEL=all
```

## 6. xclbin targets

### `make xclbin TARGET=<target>`

Purpose: link synthesized kernel objects into an FPGA binary.

What it consumes:

- kernel objects such as `saxpy_xo`
- `config/<target>/link.cfg`
- Vitis platform `.xpfm`

What it produces:

```text
build/<preset>/src/kernels/<name>_xclbin/<name>.xclbin
```

This can be slow. Run `csynth` and `cosim` first so you do not wait for link just to discover a kernel bug.

### `make analyze-link TARGET=<target> HOST_APP=<host-app>`

Purpose: summarize Vitis link/xclbin artifacts after `make xclbin` or an emulation link performed by `make swemu`/`make hwemu`.

Example:

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

What it reads:

- `.xclbin` files under the selected build directory
- Vitis link `v++.log` files
- `link.cfg` connectivity, either copied under the build tree or from `config/<target>/link.cfg`

What it shows:

- xclbin output paths and sizes
- compute-unit names such as `saxpy_1` and `vadd_1`
- memory bindings such as `saxpy_1.x -> DDR[0]` or `HBM[0]`
- clock settings
- Vitis link warnings/errors

Use this before debugging the host app. If link did not put the expected compute unit or memory binding into the xclbin, the host app cannot fix it.

### `make swemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

Purpose: build a `sw_emu` xclbin in a separate build tree and run the selected host app in software emulation.

Use it for quick host/XRT smoke tests without a physical card.

### `make hwemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

Purpose: build a `hw_emu` xclbin and run the selected host app in hardware emulation.

Use it when you want a closer host/XRT/device integration test without a physical card, assuming your platform supports emulation.

### `make qemu TARGET=<embedded-target> HOST_APP=<app> DATASET=<data>`

Purpose: build the embedded QEMU inputs and execute the board/BSP QEMU launcher.

QEMU is not the same thing as accelerator `hw_emu`. It models the embedded PS
side and depends on the platform/image QEMU launch scripts supplied with your
board support package. Set `QEMU_LAUNCHER=/path/to/qemu-launch.sh`; the launcher
receives `HOST_BIN`, `XCLBIN_PATH`, `DATA_DIR`, `RUN_DIR`, `OUTPUT`,
`EMCONFIG_PATH`, `TARGET`, `HOST_APP`, `DATASET`, and `ANVIL_PLATFORM` in its
environment. It must create `$OUTPUT`, usually
`runs/<target>/qemu/<host_app>/<dataset>/<run_key>/out.bin`.

## 7. Dataset and correctness targets

### `make gen DATASET=<name>`

Purpose: generate input files.

### `make gold DATASET=<name>`

Purpose: run the CPU reference and write expected output.

### `make compare DATASET=<name>`

Purpose: compare run output with gold output.

A typical correctness sequence is:

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

## 8. Host run and deployment targets

### `make hw TARGET=<target> HOST_APP=<app> DATASET=<data>`

Purpose: run a host app locally on a machine with an FPGA card and XRT.

It expects:

- host binary built
- xclbin built
- dataset exists
- XRT can see the device

### `make deploy-*`

Purpose: copy host binary, xclbin, data, and config to a remote embedded board.

Read [Deployment guide](deploy.md) before using these targets.

## 9. Which command should I run?

| Situation | Command |
|---|---|
| I changed normal C++ utility code | `make test` |
| I changed a host app | `make build TARGET=<target> HOST_APP=<app>` |
| I changed kernel HLS code | `make csynth TARGET=<target> KERNEL=<kernel>` |
| I changed kernel behavior | `make cosim TARGET=<target> KERNEL=<kernel>` |
| I changed link.cfg | `make xclbin TARGET=<target>` |
| I changed data format | `make gen`, `make gold`, `make swemu`/`make hwemu`/`make qemu`/`make hw`, `make compare` |
| I want report summaries | `make analyze`, `make analyze-cosim` |
| I need Python tools | `make python-env` |

## 10. Common build failures

### Platform `.xpfm` missing

Check `config/<target>/anvil.mk`. The `ANVIL_PLATFORM` path must point to an installed Vitis platform.

### XRT missing

Host builds need XRT headers and libraries. Source XRT setup or set `XILINX_XRT` if your install is not under `/opt/xilinx/xrt`.

### Embedded host compiler cannot find libc/gcc files

Your `PETALINUX_SYSROOT` does not match the compiler/toolchain. Use a sysroot from the board image or matching PetaLinux release.

### Cosim target missing

The kernel was not registered with `TESTBENCH`, or `ANVIL_COSIM_TARGETS` in `config/<target>/anvil.mk` does not include it.
