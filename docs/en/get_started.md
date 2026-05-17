# Get started: run the flow once end-to-end

This guide walks through the project in the same order an FPGA developer normally uses it. The goal is not to maximize performance — it is to understand what each stage does and to confirm your local environment works.

Before starting, read [Concepts](concepts.md) if these terms are unfamiliar: kernel, host app, xclbin, XRT, csynth, cosim, MODE.

## What you will run

The flow has four levels. Start from the top and only go deeper after the previous level passes.

| Level | Commands | Needs Vitis? | Needs XRT? | Needs FPGA hardware? | Purpose |
|---|---|:---:|:---:|:---:|---|
| CPU-only tests | `make test` | No | No | No | Verify normal C++ and Python code |
| HLS synthesis | `make csynth TARGET=... KERNEL=...` | Yes | No | No | Turn kernel C++ into hardware reports |
| HLS cosimulation | `make cosim TARGET=... KERNEL=...` | Yes | No | No | Verify generated RTL matches the C++ kernel |
| Device run | `make xclbin`, then `make swemu`/`make hwemu`/`make hw`/`make qemu` | Yes | Yes | Optional | Build and run a mode-specific FPGA binary |

**Do not skip levels.** A wrong kernel stays wrong in hardware. Fix it at `csynth` or `cosim` before waiting hours for `xclbin`.

---

## Step 1: check the repository

```bash
git status --short
```

A clean working tree is easier to debug. Build outputs go under `build/`, datasets under `data/`, and run outputs under `runs/`. None of these are committed.

---

## Step 2: run CPU-only tests

```bash
make test
```

This is the fastest sanity check. It configures a CPU-only CMake build, compiles all CPU-side code (gold reference, HLS model, tests), and runs CTest plus the Python test suite. No Vitis, no XRT, no FPGA hardware.

Expected output:

```text
100% tests passed
```

**If this fails, fix it before touching Vitis.** CPU-only failures almost always mean a normal C++ or Python problem, not an FPGA-specific one.

What this does internally:
1. Configures the `hls-model-linux-debug` CMake preset.
2. Builds CPU targets: gold, HLS model, tests, CLI utilities.
3. Runs `ctest` and `pytest`.

---

## Step 3: understand the demo kernels and layout

The default demo ships three kernels:

| Kernel | Computes | Top function |
|---|---|---|
| `saxpy` | `out[i] = a * x[i] + y[i]` | `saxpy` |
| `vadd` | `out[i] = a[i] + b[i]` | `vadd` |
| `pipeline_demo` | `saxpy_stream → vadd_stream` kernel-to-kernel pipeline | `saxpy_stream` + `vadd_stream` |

And three host apps:

| Host app | What it runs |
|---|---|
| `run_saxpy` | Loads an xclbin containing `saxpy_1` |
| `run_vadd` | Loads an xclbin containing `vadd_1` |
| `run_pipeline_demo` | Loads stream pipeline xclbin |

**Key layout:**

```
src/kernels/include/kernels/   ← Kernel ABI headers + shared core helpers (edit these)
src/kernels/                   ← Vitis HLS kernel implementations (edit these)
src/hls_model/                 ← CPU-compiled models mirroring kernel structure (edit these)
src/gold/                      ← Simple CPU truth implementations (edit these)
src/host/                      ← XRT host applications (edit these)
include/anvil/hls/             ← Framework hlslib wrappers (do not edit)
config/<target>/               ← Board/platform configuration (edit these)
```

---

## Step 4: generate a dataset and gold reference

Create the input dataset:

```bash
make gen DATASET=tiny
```

This writes files to `data/tiny/`: `meta.json`, `x.bin`, `y.bin`, and others depending on the kernel. You only need to run this once unless you change the generator or want a different size.

Generate the expected output (CPU truth):

```bash
make gold DATASET=tiny
```

This runs the scalar CPU gold reference and writes `data/tiny/gold_out.bin`. Run this after `gen`.

You will compare FPGA outputs against this gold later.

---

## Step 5: run HLS synthesis

Pick a target. For accelerator cards with Vitis 2024.2 installed, U250 is a common choice:

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

This reads `config/u250/anvil.mk` to find the Vitis platform and part, configures the CMake preset, and runs Vitis HLS compile mode for the `saxpy` kernel. Synthesis takes a few minutes.

**Important outputs:**

- HLS reports under the build tree, typically:
  ```text
  build/u250-host/src/kernels/saxpy_hls/hls/syn/report/saxpy_csynth.xml
  ```
- `.xo` kernel object: `saxpy.xo`

Read the synthesis report:

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

Look for:
- **II** — Initiation Interval. 1 is ideal. Higher values reduce throughput. The target is what your pragma requested; the achieved value is what Vitis found after analysis.
- **Timing** — Whether the design fits in the clock period.
- **Resource utilization** — How many LUT/FF/DSP/BRAM the kernel uses.

At this stage you are not running on FPGA yet — only generating hardware reports.

**If csynth fails:**
- Check the Vitis HLS log for the error.
- Common causes: HLS-incompatible C++ (templates with complex type inference, dynamic allocation), missing include path, invalid platform path.

---

## Step 6: run HLS cosimulation

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosim runs the C++ testbench from `tests/kernels/` against the RTL generated by synthesis. It verifies the RTL produces the same results as the C++ kernel. This can take longer than synthesis.

**If cosim fails:**
- Do not debug the host app yet. The kernel or testbench is the problem.
- Check whether the testbench input matches the kernel's assumptions (element count, pack alignment, scalar arguments).
- Check the cosim log for the exact simulation failure.

---

## Step 7: link an xclbin

```bash
make xclbin TARGET=u250
```

The Vitis linker takes the synthesized kernel objects (`saxpy.xo`, `vadd.xo`) and produces an FPGA binary using the platform and `config/u250/link.cfg`. This is often the slowest step for hardware builds.

The xclbin lands here:

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

Inspect the link result:

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

This shows:
- Whether the xclbin was created
- Which compute units are inside (e.g. `saxpy_1`, `vadd_1`)
- How each pointer argument is bound to memory (e.g. `saxpy_1.x → DDR[0]`)
- Clock settings
- Any Vitis link warnings or errors

**Fix link problems here before building the host app.** If the compute unit name or memory binding is wrong in the xclbin, the host app cannot fix it at runtime.

---

## Step 8: build the host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

This builds only the selected host executable without synthesizing kernels or creating a Python environment. Output:

```text
build/u250-host/src/host/run_saxpy
```

For embedded targets that need cross-compilation, see [Embedded flow](embedded_flow.md).

---

## Step 9: run on hardware or emulation

Before running emulation, generate the emulation configuration:

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

This creates `build/u250/emconfig/emconfig.json`. One file covers both `sw_emu` and `hw_emu` for the same target. You only need to run this once per target.

### Software emulation (no card needed, fast)

Software emulation runs the original C++ kernel code in a simulated XRT environment. Use it to verify the host app works correctly before committing to hardware synthesis.

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Software emulation does not require csynth. It uses a software model of the kernel, so it cannot tell you about timing or RTL correctness.

### Hardware emulation (no card needed, slower)

Hardware emulation runs RTL simulation against the synthesized kernel. It is much slower than software emulation but catches RTL-level bugs.

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Hardware emulation requires csynth to have run first. Use a small dataset — RTL simulation of large data transfers can take hours.

### Real hardware run

If an FPGA card is installed and XRT is sourced:

```bash
. /opt/xilinx/xrt/setup.sh
xbutil examine    # confirm the card is visible
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

---

## Step 10: compare results

After any run (swemu, hwemu, or hw), compare the output to the gold reference:

```bash
make compare DATASET=tiny
```

All run modes write output under `runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/`. The comparison reads from there.

A passing comparison means the FPGA (or emulation) produced the expected results.

---

## Step 11: embedded boards

Embedded targets (ZCU102, ZCU104, KV260, ...) need two extra things:

- A cross-compilation sysroot for the ARM host app
- A board filesystem with XRT installed

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux

make build-host TARGET=zcu102 HOST_APP=run_saxpy  # cross-compile for AArch64
make csynth     TARGET=zcu102 KERNEL=saxpy
make xclbin     TARGET=zcu102
make gen        DATASET=tiny
make gold       DATASET=tiny
```

Then deploy and run. See [Embedded flow](embedded_flow.md) and [Deployment guide](deploy.md).

For QEMU emulation of the embedded target:

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

The launcher is platform/image-specific and must create the output file at `$OUTPUT`.

---

## What success looks like

A complete first pass means:

- `make test` passes (CPU code is correct)
- At least one kernel passes `csynth` (Vitis accepts the code)
- The same kernel passes `cosim` (RTL matches the C++ kernel)
- A host app builds with `make build`
- `make swemu` or `make hw` produces output that matches `make compare`

After that, move to [Customization guide](customization.md) to replace the demos with your own algorithm.

---

## Common first-run problems

| Symptom | Likely cause | Fix |
|---|---|---|
| `make test` fails | Normal C++/Python bug | Read the CTest or pytest error message |
| `vitis_hls: command not found` | Vitis not sourced | `. /tools/Xilinx/Vitis/2024.2/settings64.sh` |
| `platform not found` | Wrong `.xpfm` path in `config/<target>/anvil.mk` | Check that the path exists on disk |
| csynth accepts code but cosim fails | RTL mismatch | Debug the testbench and kernel logic |
| `xbutil examine` shows no card | XRT not sourced or card not installed | `. /opt/xilinx/xrt/setup.sh`, check PCIe |
| Host app fails "cannot open xclbin" | Wrong xclbin path or MODE mismatch | Verify the xclbin exists for the right mode |
| Host app fails "kernel not found" | Compute unit name mismatch | Check `link.cfg` `nk=` vs host `GetKernel()` argument |
| Output mismatch | Kernel bug, BO group index, or data layout | Start with `make swemu` and compare with gold |
