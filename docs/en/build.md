# Build guide: every Make target explained

This page explains every Make target. Each entry says what it consumes, what it produces, and when to use it. For the caching model behind these commands, read [Build system model](build_system.md).

## The five selectors

Almost every command accepts some combination of these variables:

```bash
make <target> TARGET=u250 KERNEL=saxpy HOST_APP=run_saxpy DATASET=tiny MODE=hw
```

| Variable | Selects | Example values | Default |
|---|---|---|---|
| `TARGET` | Board/platform config from `config/<target>/anvil.mk` | `u250`, `u55c`, `zcu102`, `kv260` | `u250` |
| `KERNEL` | HLS kernel target name | `saxpy`, `vadd`, `pipeline_demo`, `all` | `all` |
| `HOST_APP` | CPU executable name under `src/host/` | `run_saxpy`, `run_vadd` | `run_saxpy` |
| `DATASET` | Dataset directory name under `data/` | `tiny` | `tiny` |
| `MODE` | Execution context | `hw`, `hw_emu`, `sw_emu` | `hw` |

Get `TARGET` right first. A wrong `TARGET` uses the wrong platform path, wrong preset, and wrong memory bank names.

---

## Fast CPU-only targets

These targets do not need Vitis or XRT. Run them first.

### `make test`

Runs the full CPU-only test suite.

```bash
make test
```

**What it does:**
1. Configures the `hls-model-linux-debug` CMake preset (native x86_64 debug build).
2. Builds CPU-side code: gold reference, HLS model, tests, CLI utilities.
3. Runs CTest for C++ tests.
4. Runs pytest for Python tool tests.

**What it produces:**
- Build artifacts in `build/hls-model-linux-debug/`
- Test pass/fail output to the terminal

**Use it when:**
- After any change to C++, Python, or data format code
- Before every HLS synthesis or cosim run
- As the first thing after `git pull`

If this fails, fix it before using Vitis.

---

## Dataset and gold targets

### `make gen DATASET=<name>`

Generates the input dataset.

```bash
make gen DATASET=tiny
```

**What it does:** Runs `scripts/gen_dataset.py` (or a compiled app) to create binary input files and `meta.json` under `data/<name>/`.

**What it produces:**
```text
data/tiny/meta.json    ← dataset parameters
data/tiny/x.bin        ← input array
data/tiny/y.bin        ← input array (if your kernel uses two inputs)
```

Run this once. Re-run only if you change the generator or want a different dataset.

### `make gold DATASET=<name>`

Runs the CPU gold reference and writes the expected output.

```bash
make gold DATASET=tiny
```

**Depends on:** `make gen` for the same dataset.

**What it produces:**
```text
data/tiny/gold_out.bin  ← expected kernel output
```

Compare all FPGA run outputs against this file.

---

## HLS synthesis targets

### `make csynth TARGET=<target> KERNEL=<kernel>`

Turns kernel C++ into HLS output: reports, resource estimates, and a compiled kernel object.

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=all      # synthesize all kernels
```

**Consumes:**
- Kernel sources: `src/kernels/<kernel>_kernel.cpp`
- ABI headers: `src/kernels/include/kernels/`
- Platform: path from `config/<target>/anvil.mk`

**Produces:**
- HLS work directory with synthesis logs
- `.xo` kernel object (compiled kernel)
- `<kernel>_csynth.xml` report with II, timing, and resource estimates

**When it fails:**
- Check the Vitis HLS log. Common errors:
  - Kernel uses C++ features HLS does not support (dynamic memory, virtual functions, complex templates)
  - Include path is wrong or a header is missing
  - Platform path in `anvil.mk` does not exist on disk

### `make analyze TARGET=<target> KERNEL=<kernel>`

Reads existing csynth reports and shows a human-friendly summary.

```bash
make analyze TARGET=u250 KERNEL=saxpy
make analyze TARGET=u250 KERNEL=all
```

Does not run synthesis. Reads what is already under the build tree.

**What it shows:**
- Achieved Initiation Interval (II) vs. target II
- Estimated clock period and timing margin
- LUT, FF, DSP, BRAM, URAM utilization (count and percentage of device total)
- Warnings about II violations or timing failures

Use this immediately after `csynth`. Do not proceed to cosim or xclbin if II or timing is severely off target.

---

## HLS cosimulation targets

### `make cosim TARGET=<target> KERNEL=<kernel>`

Runs C/RTL cosimulation: the C++ testbench drives the synthesized RTL.

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=all
```

**Consumes:**
- Synthesized kernel (must run `csynth` first)
- Testbench: `tests/kernels/<kernel>_cosim_tb.cpp`

**Produces:**
- Cosim log and report files under the HLS work directory
- Pass/fail status
- Transaction-level latency statistics

**Cosim is a kernel test.** It does not run a host app, does not load an xclbin, and does not use XRT. If cosim fails, fix the kernel or testbench before proceeding.

### `make analyze-cosim TARGET=<target> KERNEL=<kernel>`

Reads cosim reports and shows a summary.

```bash
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Shows pass/fail, latency, and any simulation errors.

---

## xclbin targets

### `make xclbin TARGET=<target>`

Links synthesized kernel objects into a complete FPGA binary.

```bash
make xclbin TARGET=u250           # hardware xclbin (MODE=hw by default)
make xclbin TARGET=u250 MODE=hw_emu
make xclbin TARGET=u250 MODE=sw_emu
```

**Consumes:**
- Kernel objects (`.xo`) from synthesis
- `config/<target>/link.cfg` — connectivity rules (compute units, memory bindings, clocks)
- Platform `.xpfm` from `config/<target>/anvil.mk`

**Produces:**
```text
build/<preset>/src/kernels/<name>_xclbin/<name>.xclbin
build/<preset>/src/kernels/<name>_xclbin/.link.stamp
```

Each mode produces a different xclbin. `sw_emu`, `hw_emu`, and `hw` xclbins are not interchangeable.

This step is often slow for hardware builds. Run `csynth` and `cosim` first to avoid waiting for link just to find a kernel bug.

### `make analyze-link TARGET=<target> HOST_APP=<app>`

Reads Vitis link artifacts and shows what ended up in the xclbin.

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

**What it shows:**
- xclbin file path and size
- Compute unit names (e.g. `saxpy_1`, `vadd_1`)
- Memory port bindings (e.g. `saxpy_1.x → DDR[0]`)
- Requested and achieved clock settings
- Any Vitis link warnings or errors

Use this before debugging a host app at runtime. If the xclbin does not contain the expected compute unit or memory binding, the host app cannot fix it.

### `make emconfig TARGET=<target> MODE=<mode>`

Generates the emulation configuration file required by XRT for `sw_emu` and `hw_emu` runs.

```bash
make emconfig TARGET=u250 MODE=sw_emu
make emconfig TARGET=u250 MODE=hw_emu  # same file, same target
```

**Produces:**
```text
build/<target>/emconfig/emconfig.json
```

One `emconfig.json` serves both software and hardware emulation for the same target. You only need to run this once per target. Run it before `swemu` or `hwemu`.

The `make swemu` and `make hwemu` targets depend on this file and will build it automatically if missing.

---

## Build targets

### `make build TARGET=<target> HOST_APP=<app>`

Builds one host executable.

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

**Consumes:**
- Host source: `src/host/<HOST_APP>.cpp`
- XRT headers and libraries from `ANVIL_XRT_LIB` in `anvil.mk`
- Kernel ABI headers from `src/kernels/include/kernels/`

**Produces:**
```text
build/<preset>/src/host/<HOST_APP>
```

Does not synthesize kernels. Does not create a Python environment. This is fast.

For embedded targets that need cross-compilation:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

---

## Run targets

These targets require all dependencies (xclbin, host binary, dataset) to be built first.

### `make swemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

Runs the host app in XRT software emulation.

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**What it does:**
- Builds a `sw_emu` xclbin if not already built
- Ensures `emconfig.json` exists
- Sets `XCL_EMULATION_MODE=sw_emu` and `EMCONFIG_PATH=build/<target>/emconfig`
- Runs the host app

Software emulation replaces the FPGA with a fast behavioral C model of the kernel. It does not run RTL simulation. It is useful for checking host app correctness without waiting for hardware synthesis.

**Run output:** `runs/<target>/sw_emu/<host_app>/<dataset>/<run_key>/`

### `make hwemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

Runs the host app in XRT hardware emulation.

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**What it does:**
- Builds a `hw_emu` xclbin using synthesized kernel RTL
- Ensures `emconfig.json` exists
- Sets `XCL_EMULATION_MODE=hw_emu` and `EMCONFIG_PATH=build/<target>/emconfig`
- Runs the host app against RTL simulation

Hardware emulation is slower than software emulation because it runs actual RTL simulation. Use a small dataset. It catches RTL correctness issues that software emulation misses.

**Requires:** `csynth` must have run first.

**Run output:** `runs/<target>/hw_emu/<host_app>/<dataset>/<run_key>/`

### `make hw TARGET=<target> HOST_APP=<app> DATASET=<data>`

Runs the host app on a physical FPGA card.

```bash
. /opt/xilinx/xrt/setup.sh
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**Requires:** XRT sourced, card visible to `xbutil examine`, hardware xclbin built.

**Run output:** `runs/<target>/hw/<host_app>/<dataset>/<run_key>/`

### `make qemu TARGET=<embedded-target> HOST_APP=<app> DATASET=<data> QEMU_LAUNCHER=<script>`

Runs the embedded target under QEMU.

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

QEMU is not the same as accelerator `hw_emu`. It models the ARM processor side of ZynqMP boards and requires a platform/image-specific launcher script. The launcher receives environment variables: `HOST_BIN`, `XCLBIN_PATH`, `DATA_DIR`, `RUN_DIR`, `OUTPUT`, `EMCONFIG_PATH`, `TARGET`, `HOST_APP`, `DATASET`, `ANVIL_PLATFORM`. It must create `$OUTPUT`.

**Run output:** `runs/<target>/qemu/<host_app>/<dataset>/<run_key>/`

---

## Correctness targets

### `make compare DATASET=<name>`

Compares the most recent run output against the gold reference.

```bash
make compare DATASET=tiny
```

Reads `data/<dataset>/gold_out.bin` and the output from the last run. Prints pass/fail and error statistics.

### `make compare-hls KERNEL=<kernel> DATASET=<name>`

Compares HLS model output against gold.

```bash
make compare-hls KERNEL=saxpy DATASET=tiny
```

Use this after `make test` when you want to verify the HLS model numerically matches the gold reference.

---

## Python environment target

### `make python-env`

Creates the project Python virtual environment.

```bash
make python-env
```

Uses `uv` if available, otherwise `python3 -m venv`. Installs test dependencies. Required for `make analyze`, `make analyze-cosim`, `make analyze-link`, and Python tests.

Disable the default mirror:

```bash
make python-env PYPI_INDEX=
```

Rebuild from scratch:

```bash
make rebuild-python
```

---

## Deployment targets

For deploying to remote embedded boards, see [Deployment guide](deploy.md).

```bash
make deploy-bin    TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=192.168.1.10
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.10
make deploy-data   TARGET=zcu102 DATASET=tiny BOARD_IP=192.168.1.10
make deploy-check  TARGET=zcu102 BOARD_IP=192.168.1.10
make test-hw       TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=192.168.1.10
```

---

## Decision table: which command should I run?

| Situation | Command |
|---|---|
| Changed C++ logic or Python tools | `make test` |
| Changed kernel HLS code | `make csynth TARGET=<t> KERNEL=<k>` then `make analyze` |
| Want to verify RTL matches C++ kernel | `make cosim TARGET=<t> KERNEL=<k>` |
| Changed `link.cfg` | `make xclbin TARGET=<t>` |
| Changed host app | `make build TARGET=<t> HOST_APP=<app>` |
| Changed dataset format | `make gen` and `make gold` |
| Want a quick no-card smoke test | `make swemu TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| Want RTL-level no-card test | `make hwemu TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| Run on real hardware | `make hw TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| Check report numbers | `make analyze`, `make analyze-cosim`, `make analyze-link` |
| Set up Python tools | `make python-env` |

---

## Common build failures

### `vitis_hls: command not found` or `v++: command not found`

Vitis is not in PATH. Source it:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

### Platform `.xpfm` file not found

The `ANVIL_PLATFORM` path in `config/<target>/anvil.mk` does not exist. Install the platform or fix the path.

### XRT headers not found during host build

XRT is not installed or `ANVIL_XRT_LIB` in `anvil.mk` points to the wrong location. Typical default: `/opt/xilinx/xrt`. Source XRT:

```bash
. /opt/xilinx/xrt/setup.sh
```

### Embedded cross-compile fails: `crtbeginS.o` or `-lgcc` not found

The sysroot does not match the compiler. Use a sysroot from the PetaLinux release that matches your board image and Vitis version.

### Cosim target not found for kernel

The kernel was registered without a `TESTBENCH` argument in `CMakeLists.txt`, or `ANVIL_COSIM_TARGETS` in `anvil.mk` does not include it.

### `make xclbin` says "kernel not found"

The kernel name in `link.cfg` (`nk=<name>`) does not match the synthesized kernel top function name. Check that `nk=saxpy:1:saxpy_1` matches `extern "C" void saxpy(...)`.
