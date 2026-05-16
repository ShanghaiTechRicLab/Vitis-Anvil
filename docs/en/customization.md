# Customization guide

Use Vitis-Anvil as a template: keep the flow, replace the demo pieces.

## 1. Add or replace a kernel

Kernel source lives in `src/kernels/`. Register a kernel in `src/kernels/CMakeLists.txt`:

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

If you have a cosim testbench:

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

Host apps live in `src/host/`. Add an executable target in `src/host/CMakeLists.txt`, link the libraries you need, and run it with `HOST_APP=`:

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

Use the runtime helpers:

```cpp
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/xrt_buffer.hpp>

anvil::runtime::XrtContext ctx{0, xclbin_path};
auto kernel = ctx.GetKernel("my_kernel:{my_kernel_1}");
```

## 3. Add xclbin connectivity

Add or edit `config/<target>/link.cfg`:

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

Then add matching configure/build/test presets in `CMakePresets.json`. For embedded devices set:

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

## 5. Add platform metadata for analysis

`hlsflow` device notes live in `tools/hlsflow/platform_info.py`. Add resource totals, memory notes, and default clock so reports can show meaningful headroom.

## 6. Customize datasets and gold references

Dataset generation:

- `scripts/gen_dataset.py`
- output under `data/<dataset>/`

Gold references:

- C++: `src/gold/cpp/`
- Python: `src/gold/python/`

Comparison:

- `scripts/compare.py`

Keep the contract simple: host apps should write output files under `data/<dataset>/`, and `make compare` should compare those against gold output.

## 7. Customize thresholds

Use `hlsflow check` through Make:

```bash
make check-hls
```

Or directly:

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000
```

## 8. Keep command semantics clean

- `TARGET=` selects board/platform.
- `KERNEL=` selects HLS kernel targets and HLS reports.
- `HOST_APP=` selects XRT host executables.
- `DATASET=` selects generated input/output data.

Do not overload `HOST_APP` to select cosim; cosim is kernel/testbench-level.
