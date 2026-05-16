# Customization guide

Vitis-Anvil is meant to be used as a template. Keep the build flow and the tooling; replace the demo pieces with your own kernels and host apps.

## 1. Add or replace a kernel

**What you touch:** `src/kernels/` and `src/kernels/CMakeLists.txt`

Write your kernel C++ code in `src/kernels/`. Then register it in `src/kernels/CMakeLists.txt` using `add_anvil_kernel()`:

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

If you have a cosim testbench (stored in `tests/kernels/`), add it with `TESTBENCH`:

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  TESTBENCH     ${CMAKE_SOURCE_DIR}/tests/kernels/my_kernel_cosim_tb.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

Run synthesis and cosim:

```bash
make csynth TARGET=u250 KERNEL=my_kernel
make cosim TARGET=u250 KERNEL=my_kernel
```

**Related:**
- `include/anvil/kernels/` — add a header with your kernel's ABI types
- `tests/kernels/` — add your cosim testbench here
- `config/<target>/link.cfg` — if your kernel needs connectivity settings

## 2. Add a host app

**What you touch:** `src/host/` and `src/host/CMakeLists.txt`

Write your host program in `src/host/`. Register it in `src/host/CMakeLists.txt` using `add_anvil_host()`:

```cmake
add_anvil_host(run_my_kernel
  LIBS anvil_runtime anvil_gold anvil_log anvil_cli)
```

Build and run:

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

**Related:**
- `src/anvil/runtime/` — the `XrtContext` and buffer helpers your host app links against
- `include/anvil/runtime/` — headers for those helpers (`xrt_context.hpp`, `xrt_buffer.hpp`)

## 3. Add xclbin connectivity

**What you touch:** `config/<target>/link.cfg` and `src/kernels/CMakeLists.txt`

Edit or create `config/<target>/link.cfg`:

```ini
[connectivity]
nk=my_kernel:1:my_kernel_1
sp=my_kernel_1.input:DDR[0]
sp=my_kernel_1.output:DDR[1]

[clock]
freqHz=300000000:my_kernel_1
```

Then register the xclbin in `src/kernels/CMakeLists.txt`:

```cmake
add_anvil_xclbin(
  NAME my_kernel
  KERNEL_TARGETS my_kernel_xo
  LINK_CFG ${CMAKE_SOURCE_DIR}/config/u250/link.cfg
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

**Related:**
- `config/<target>/pipeline_demo.cfg` — if you are building a streaming pipeline with kernel-to-kernel connections

## 4. Add a device

**What you touch:** `config/<target>/`, `CMakePresets.json`, and optionally `platforms/`

Create `config/my_board/anvil.mk`. This is the Makefile fragment that tells the build system about your board:

```make
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu250-figd2104-2L-e
ANVIL_PLATFORM         := /path/to/platform.xpfm
ANVIL_PRESET           := my-board-host
ANVIL_HWEMU_PRESET     := my-board-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := my_kernel_xo
ANVIL_COSIM_TARGETS    := my_kernel_cosim
```

Then add matching configure/build/test presets in `CMakePresets.json`.

For embedded devices (ZynqMP-style), use these settings instead in your `config/<target>/anvil.mk`:

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

**Related files:**
- `config/<target>/xrt.ini` — XRT runtime flags for the board
- `config/<target>/README.md` — board-specific notes you should write
- `platforms/<board>/` — board metadata in TOML format for Python tools
- `cmake/Toolchain-aarch64-linux.cmake` — cross-compilation toolchain, if you need to customize it

## 5. Add platform metadata for analysis

**What you touch:** `tools/hlsflow/platform_info.py`

Device information for `hlsflow` (resource totals, memory notes, default clock) lives in `tools/hlsflow/platform_info.py`. Adding your board here lets the analysis reports show meaningful headroom percentages.

## 6. Customize datasets and golden references

**What you touch:** `scripts/gen_dataset.py`, `src/gold/`, `scripts/compare.py`

The data pipeline looks like this:

| Step | Script | Called by | Writes to |
|---|---|---|---|
| Generate input | `scripts/gen_dataset.py` | `make gen` | `data/<dataset>/` |
| Gold reference | `src/gold/cpp/` or `src/gold/python/` | `make gold` | `data/<dataset>/` |
| Hardware run | host app (e.g. `src/host/run_saxpy.cpp`) | `make run-host` | `data/<dataset>/` |
| Compare | `scripts/compare.py` | `make compare` | stdout |

To customize the data format:
1. Edit `scripts/gen_dataset.py` to change what gets generated.
2. Update the C++ gold code in `src/gold/cpp/` or the Python gold code in `src/gold/python/`.
3. Update `scripts/compare.py` if the comparison logic needs to change.

The contract is simple: host apps write output files under `data/<dataset>/`, and `make compare` checks those files against the golden reference.

## 7. Customize thresholds

**What you touch:** the `hlsflow check` command (via Make or directly)

Use `hlsflow check` through Make:

```bash
make check-hls
```

Or run it directly with custom limits for any of these values:

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000 --max-bram 256
```

The threshold parameters are not stored in a config file — you pass them on the command line. This keeps the check explicit and reviewable.

## 8. Keep the command semantics clean

- `TARGET=` picks the board or platform (`config/<target>/anvil.mk`).
- `KERNEL=` picks HLS kernel targets (`src/kernels/CMakeLists.txt`) and HLS reports.
- `HOST_APP=` picks the XRT host executable (`src/host/CMakeLists.txt`).
- `DATASET=` picks the input and output data (`data/<dataset>/`).

Do not overload `HOST_APP` for cosim or kernel selection. Cosim is a kernel-level concept and belongs under `KERNEL`.
