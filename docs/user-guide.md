# User Guide — Replacing the saxpy Demo

Vitis-Anvil ships with saxpy as the reference demo. This guide shows the demo
touch-points to change for your own kernel.

## Demo touch-points

### 1. `src/kernels/saxpy_kernel.cpp` — your HLS kernel

Replace `saxpy_kernel.cpp` with your HLS C++ kernel source. Update
`src/kernels/CMakeLists.txt` if you change the filename or top function. The
Makefile currently expects the CMake target names `saxpy_xo`, `saxpy_cosim`,
`saxpy_xclbin`, and `run_saxpy`; either keep `NAME saxpy` while replacing the
implementation, or update the Makefile variables/targets and the
`add_anvil_xclbin(...)` registration together.

```cmake
add_anvil_kernel(
  NAME          saxpy        # keep this unless you also update Makefile target names
  TOP           my_kernel_top
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

### 2. `src/host/run_saxpy.cpp` — your XRT host

Replace the saxpy demo with your kernel's argument layout:

- Change `ctx.GetKernel("saxpy:{saxpy_1}")` to your kernel/CU string, e.g.
  `ctx.GetKernel("my_kernel_top:{my_kernel_1}")`.
- Change `XrtBuffer` sizes and types to match your kernel args.
- Load your dataset format from `--data-dir`.
- Write your device output to `--output` so `make compare` can consume it.
- Update the gold verification call.

### 3. `src/hls_model/saxpy_hls_model.cpp` — CPU parity model (optional)

Implement a CPU version of your kernel for `make test` verification. The
repository Day-0 `make test` path intentionally uses the `hls-model-linux-debug`
preset, where `ANVIL_BUILD_HLS_MODEL=ON`; if you remove the HLS model, also
update every preset/build flow that enables `ANVIL_BUILD_HLS_MODEL`
(`hls-model-linux-*`, `u250-host`, and inherited `u250-host-hwemu` today),
`src/hls_model/CMakeLists.txt`, and tests that link `anvil_hls_model`.

### 4. `src/gold/cpp/saxpy_gold.cpp` — C++ gold reference

Replace `saxpy_gold()` with your reference algorithm. Update
`include/anvil/gold/saxpy_gold.hpp` if the signature changes.

### 5. `src/gold/python/saxpy_gold.py` — Python gold reference

Replace the `saxpy_gold()` NumPy implementation with your algorithm. This is
called by `make gold ANVIL_LANG=python`.

### 6. `config/<target>/link.cfg` — kernel port→memory/interface mapping

Update the `nk=`, `sp=`, and `[clock] freqHz=...:<cu>` lines to match your
kernel top name, CU name, port names, and target memory interfaces. U250 uses
DDR banks; ZCU104 uses platform-specific HP interfaces.

```ini
[connectivity]
nk=my_kernel_top:1:my_kernel_1
sp=my_kernel_1.input_a:DDR[0]
sp=my_kernel_1.input_b:DDR[1]
sp=my_kernel_1.output:DDR[2]

[clock]
freqHz=300000000:my_kernel_1
```

### 7. `scripts/gen_dataset.py` — input data format

Change the data generation logic to produce inputs for your kernel. The current
saxpy demo writes `x.bin`, `y.bin`, and `meta.json`; your template can use any
format if all consumers agree.

### 8. `scripts/compare.py` — output comparison format

Change the output loaders and metric columns to match your kernel. `make compare`
compares outputs that have already been produced under `data/<dataset>/`.

## Workflow after replacing the demo

```bash
make build TARGET=u250       # configure + build C++/Python and U250 csynth
make test                    # CPU-only fast tests
make csynth TARGET=u250      # HLS synthesis
make cosim TARGET=u250       # HLS co-simulation where tests are enabled
make xclbin-hwemu TARGET=u250
make gen DATASET=small
make gold ANVIL_LANG=python DATASET=small
make xrt-emu TARGET=u250 DATASET=small
make compare DATASET=small   # compare outputs already produced for the dataset
make xclbin TARGET=u250      # hardware link; long-running
make xrt-hw TARGET=u250 DATASET=small
```

## Adding a second target device

1. Create `config/my_device/anvil.mk` following the U250 or ZCU104 template.
2. Create `config/my_device/link.cfg` for your kernel's port/memory mapping.
3. Add CMake configure/build/test presets in `CMakePresets.json`.
4. Run `make build TARGET=my_device`.

## Using the anvil C++ library in your files

```cpp
#include <anvil/log/anvil_log.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/runtime/xrt_context.hpp>

anvil::log::Init("my_host");
double err = anvil::compare::MaxAbsError(gold, result);
```

Link with the CMake aliases you need:

```cmake
target_link_libraries(my_host PRIVATE anvil::log anvil::compare anvil::runtime)
```

## Using the anvil Python library

```python
import anvil.log as log
import anvil.compare as cmp

log.init("my_script")
err = cmp.max_abs_error(gold, result)
log.info("max_abs_error = {:.3e}", err)
```
