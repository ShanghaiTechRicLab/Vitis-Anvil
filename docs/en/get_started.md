# Get started: run the flow once

This guide walks through the project in the same order an FPGA developer normally uses it. The goal is not to get maximum performance; the goal is to understand what each stage does and to prove your local setup works.

Before starting, read [Concepts](concepts.md) if these words are new: kernel, host app, xclbin, XRT, csynth, cosim.

## 1. What you will run

You will run four levels of work:

| Level | Command | Needs Vitis? | Needs XRT? | Needs FPGA hardware? | Purpose |
|---|---|---:|---:|---:|---|
| CPU-only tests | `make test` | No | No | No | Check normal C++/Python code and examples |
| HLS synthesis | `make csynth TARGET=... KERNEL=...` | Yes | No | No | Check Vitis can turn kernel C++ into hardware |
| HLS cosim | `make cosim TARGET=... KERNEL=...` | Yes | No | No | Check generated RTL behaves like the C++ kernel |
| Hardware run | `make xclbin`, `make run-host` or deploy | Yes | Yes | Yes or hw_emu | Build/load FPGA binary and execute it |

Do not start with hardware. Start with CPU-only tests, then synthesis, then cosim, then xclbin/host.

## 2. Check the repository

From the repository root:

```bash
git status --short
```

A clean tree is easier to debug. Build outputs go under `build/`, generated data under `data/`, reports under `reports/`.

## 3. Run CPU-only tests

```bash
make test
```

What this does:

1. Configures the `hls-model-linux-debug` preset.
2. Builds CPU-side code: gold reference, HLS model, CLI utilities, tests.
3. Runs CTest.

What it does **not** do:

- no Vitis synthesis
- no xclbin link
- no XRT device access
- no FPGA hardware run

Expected result:

```text
100% tests passed
```

If this fails, fix it before touching Vitis. CPU-only failures usually mean normal C++/Python or data-format problems.

## 4. Understand the demo target names

The default demo has these kernels:

| Kernel | What it computes | Where it lives |
|---|---|---|
| `saxpy` | `out = a*x + y` | `src/kernels/saxpy_kernel.cpp` |
| `vadd` | `out = a + b` | `src/kernels/vadd_kernel.cpp` |
| `pipeline_demo` | `saxpy_stream -> vadd_stream` | stream kernels in `src/kernels/` |

The default host apps are:

| Host app | What it runs |
|---|---|
| `run_saxpy` | loads an xclbin containing `saxpy_1` |
| `run_vadd` | loads an xclbin containing `vadd_1` |
| `run_pipeline_demo` | loads stream pipeline xclbin |

## 5. Run HLS synthesis

Pick a target. If you have Vitis 2024.2 installed and a platform exists, U250 is a common accelerator-card target:

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

What this does:

1. Reads `config/u250/anvil.mk` to find the Vitis platform and part.
2. Configures the CMake preset for that target.
3. Runs Vitis HLS compile mode for the `saxpy` kernel.
4. Writes HLS work directories and reports under `build/<preset>/src/kernels/`.

The important output is the csynth XML report, usually under a path like:

```text
build/u250-host/src/kernels/saxpy_hls/hls/syn/report/saxpy_csynth.xml
```

Now inspect it with:

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
```

The report tells you timing, II, latency, and resource use. At this stage you are not running on the FPGA yet.

## 6. Run HLS cosimulation

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

What this does:

1. Builds or reuses the HLS kernel output.
2. Runs a C++ testbench from `tests/kernels/` against generated RTL.
3. Produces cosim reports.
4. Extracts pass/fail and latency information.

If cosim fails, do not debug XRT or host code yet. The kernel or its testbench is wrong.

## 7. Link an xclbin

```bash
make xclbin TARGET=u250
```

What this does:

1. Uses synthesized kernel objects such as `saxpy_xo` and `vadd_xo`.
2. Reads `config/u250/link.cfg` for compute-unit names and memory-bank bindings.
3. Calls Vitis linker.
4. Produces an xclbin under the build tree.

Typical output path:

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

After link, inspect what Vitis produced:

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

This parses the link artifacts and shows the xclbin path, compute units, memory-bank connectivity from `link.cfg`, clock settings, and Vitis link warnings/errors. It answers: did the xclbin get produced, did `saxpy_1` enter it, and do the ports bind to the expected DDR/HBM banks?

This step can be slow. It is normal for hardware builds to take much longer than CPU tests.

## 8. Build the host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

What this does:

1. Configures the host CMake preset.
2. Builds only the selected host executable.
3. Does not run synthesis and does not create Python environments.

The host binary is usually:

```text
build/u250-host/src/host/run_saxpy
```

## 9. Generate input and gold output

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

What this does:

- `make gen` creates input files and `meta.json` under `data/tiny/`.
- `make gold` runs the CPU reference and writes expected output.

The FPGA run should later write hardware output into the same dataset directory so the compare step can check it.

## 10. Run on hardware or hardware emulation

If a supported FPGA card is installed and XRT is sourced, run:

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

This calls the host app with the xclbin path and dataset path. The host app loads the xclbin, copies input buffers, starts the kernel, reads output, and writes a result file.

Then compare:

```bash
make compare DATASET=tiny
```

If you are using hardware emulation instead, read the target-specific notes in [Accelerator-card flow](accelerator_flow.md).

## 11. For embedded boards

Embedded boards need two more concepts:

- a cross-compilation sysroot for the host app
- a board filesystem/image with XRT installed

Example:

```bash
PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux \
  make build-host TARGET=zcu102 HOST_APP=run_saxpy

make csynth TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
```

Then deploy files to the board. See [Embedded flow](embedded_flow.md) and [Deployment guide](deploy.md).

## 12. What success looks like

A successful first pass means:

- `make test` passes.
- At least one kernel passes `csynth`.
- The same kernel passes `cosim`.
- You can build a host app.
- If hardware is available, host output matches gold output.

After that, move to [Customization guide](customization.md) to replace the demos with your own algorithm.
