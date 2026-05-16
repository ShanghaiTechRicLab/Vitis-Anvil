# Customization guide

Vitis-Anvil is meant to be used as a template. Keep the build flow and the tooling; replace the demo pieces with your own kernels and host apps.

## 1. Add or replace a kernel

Kernel source goes in `src/kernels/`. Register it in `src/kernels/CMakeLists.txt`:

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

If you have a cosim testbench, add it with `TESTBENCH`:

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  TESTBENCH     ${CMAKE_SOURCE_DIR}/tests/kernels/my_kernel_cosim_tb.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

Then run:

```bash
make csynth TARGET=u250 KERNEL=my_kernel
make cosim TARGET=u250 KERNEL=my_kernel
```

## 2. Add a host app

Host apps live in `src/host/`. Add an executable target in `src/host/CMakeLists.txt`, link whatever libraries you need, and select it at build time with `HOST_APP=`:

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

The runtime helpers you will likely need:

```cpp
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/xrt_buffer.hpp>

anvil::runtime::XrtContext ctx{0, xclbin_path};
auto kernel = ctx.GetKernel("my_kernel:{my_kernel_1}");
```

## 3. Add xclbin connectivity

Edit or create `config/<target>/link.cfg`:

```ini
[connectivity]
nk=my_kernel:1:my_kernel_1
sp=my_kernel_1.input:DDR[0]
sp=my_kernel_1.output:DDR[1]

[clock]
freqHz=300000000:my_kernel_1
```

Register the xclbin in `src/kernels/CMakeLists.txt`:

```cmake
add_anvil_xclbin(
  NAME my_kernel
  KERNEL_TARGETS my_kernel_xo
  LINK_CFG ${CMAKE_SOURCE_DIR}/config/u250/link.cfg
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

## 4. Add a device

Create `config/my_board/anvil.mk`:

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

For embedded devices:

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

## 5. Add platform metadata for analysis

Device information for `hlsflow` lives in `tools/hlsflow/platform_info.py`. Adding resource totals, memory notes, and the default clock frequency lets the reports show meaningful headroom numbers.

## 6. Customize datasets and golden references

Dataset generation:

- `scripts/gen_dataset.py` — defines the data format
- Output goes to `data/<dataset>/`

Golden references:

- C++: `src/gold/cpp/`
- Python: `src/gold/python/`

Comparison:

- `scripts/compare.py` — compares hardware output against gold

Keep the contract simple: host apps write output files under `data/<dataset>/`, and `make compare` checks those files against the golden reference.

## 7. Customize thresholds

Use `hlsflow check` through Make:

```bash
make check-hls
```

Or run it directly with custom limits:

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000
```

## 8. Keep the command semantics clean

- `TARGET=` picks the board or platform.
- `KERNEL=` picks HLS kernel targets and HLS reports.
- `HOST_APP=` picks the XRT host executable.
- `DATASET=` picks the input and output data.

Do not overload `HOST_APP` for cosim or kernel selection. Cosim is a kernel-level concept and belongs under `KERNEL`.
