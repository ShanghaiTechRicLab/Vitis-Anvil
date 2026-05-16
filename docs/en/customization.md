# Customization guide

This page explains how to turn Vitis-Anvil from the demo project into your own accelerator project. It is intentionally concrete: it shows which files to edit, which files not to edit, what each CMake/Make variable means, and a complete example that adds a new kernel named `scaleadd`.

If you only remember one rule, remember this:

> `include/anvil/**` and `src/anvil/**` are framework code. Your project code goes under `src/kernels/**`, `src/hls_model/**`, `src/gold/**`, `src/host/**`, `src/apps/**`, `config/**`, and `tests/**`.

## 0. Mental model: what you are customizing

Vitis-Anvil separates a hardware project into several layers. Customize the right layer and the build stays predictable.

| Layer | Typical path | What belongs there | User editable? |
|---|---|---|---|
| Framework public API | `include/anvil/**` | Generic helpers: logging, JSON, compare, runtime wrappers, generic HLS helpers | Usually no |
| Framework implementation | `src/anvil/**` | Implementation of the framework libraries | Usually no |
| Kernel ABI headers | `src/kernels/include/kernels/**` | Pack types, `extern "C"` declarations, stream kernel declarations | Yes |
| Kernel implementations | `src/kernels/*.cpp` | Vitis HLS C++ kernels | Yes |
| HLS/CPU models | `src/hls_model/**` | CPU-compiled model that mirrors the kernel algorithm | Yes |
| Golden references | `src/gold/**` | Simple CPU truth model and metrics | Yes |
| Host applications | `src/host/**` | XRT programs that load xclbin and run kernels | Yes |
| Utility apps | `src/apps/**`, `scripts/**` | Dataset generation, comparison, deployment, analysis helpers | Yes |
| Board/platform config | `config/<target>/**`, `platforms/**` | xpfm path, part, sysroot, link.cfg, xrt.ini, metadata | Yes |
| Tests | `tests/**` | C++ tests, Python tests, cosim testbenches, install smoke | Yes |

The framework helpers under `include/anvil/hls/` are reusable. They are safe to include from your kernels:

```cpp
#include "anvil/hls/pack.hpp"
#include "anvil/hls/stream.hpp"
#include "anvil/hls/dataflow.hpp"
#include "anvil/hls/packed_ops.hpp"
#include "anvil/hls/axis.hpp"
```

Your kernel-specific types should not be added to `include/anvil/**`. Put them under `src/kernels/include/kernels/` instead.

## 1. The four selectors: TARGET, KERNEL, HOST_APP, DATASET

Most day-to-day commands are controlled by four variables:

| Variable | Meaning | Examples | Used by |
|---|---|---|---|
| `TARGET` | Board/platform configuration from `config/<target>/anvil.mk` | `u250`, `u50`, `zcu102`, `kv260` | configure, build, xclbin, deploy |
| `KERNEL` | HLS kernel target name | `saxpy`, `vadd`, `pipeline_demo`, `all` | `make csynth`, `make cosim`, analysis |
| `HOST_APP` | Host executable name from `src/host/CMakeLists.txt` | `run_saxpy`, `run_vadd`, `run_pipeline_demo` | host build/run/deploy |
| `DATASET` | Dataset directory under `data/<dataset>/` | `tiny`, `my_case_001` | generation, gold, compare, host runs |

Examples:

```bash
make test
make build TARGET=u250 HOST_APP=run_saxpy
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=vadd
make xclbin TARGET=u250
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make analyze-flow TARGET=zcu102 KERNEL=all
make analyze-cosim TARGET=zcu102 KERNEL=vadd
```

Keep the meanings separate. Do not use `HOST_APP` to select a cosim target. Cosim is a kernel-level operation, so it uses `KERNEL`.

## 2. Recommended workflow for a real project

Use this order when adding your own accelerator. It avoids waiting for slow Vitis runs before the CPU code is correct.

1. **Define the kernel ABI** under `src/kernels/include/kernels/`.
2. **Write the CPU/gold reference** under `src/gold/`.
3. **Write a CPU test** under `tests/cpp/` for the reference and data layout.
4. **Write the HLS kernel** under `src/kernels/`.
5. **Write a cosim testbench** under `tests/kernels/`.
6. **Register the kernel** in `src/kernels/CMakeLists.txt`.
7. **Run fast host-side tests** with `make test` or `ctest`.
8. **Run HLS synthesis** with `make csynth TARGET=<board> KERNEL=<kernel>`.
9. **Run HLS cosim** with `make cosim TARGET=<board> KERNEL=<kernel>`.
10. **Add xclbin connectivity** in `config/<target>/link.cfg`.
11. **Link xclbin** with `make xclbin TARGET=<board>`.
12. **Write or adapt a host app** under `src/host/`.
13. **Run hardware or hardware emulation**.
14. **Analyze reports** with `make analyze-flow` and `make analyze-cosim`.
15. **Only then tune performance**: clock, pack width, memory banks, dataflow depth.

Do not start by editing `src/anvil/runtime/` or `include/anvil/runtime/`. Most projects do not need to touch the runtime wrapper.

## 3. Complete example: add a `scaleadd` kernel

The example kernel computes:

```text
out[i] = alpha * a[i] + beta * b[i]
```

It is deliberately close to `saxpy` and `vadd`, but it is a new kernel with two scalar coefficients. This example shows the full path: ABI header, kernel implementation, cosim testbench, CMake registration, xclbin connectivity, host app, and commands.

### 3.1 Add the ABI header

Create `src/kernels/include/kernels/scaleadd.hpp`:

```cpp
#pragma once

#include "anvil/hls/pack.hpp"
#include "kernels/kernel_types.hpp"

namespace kernels {

static const int kScaleAddPackWidth = 16;
typedef anvil::hls::Pack<float, kScaleAddPackWidth> ScaleAddPack;

}  // namespace kernels

extern "C" void scaleadd(const kernels::ScaleAddPack* a,
                         const kernels::ScaleAddPack* b,
                         kernels::ScaleAddPack* out,
                         float alpha,
                         float beta,
                         int n_packs);
```

Why this file exists:

- It is the ABI contract between kernel, host, and cosim testbench.
- It lives under `src/kernels/include/kernels/` because it is project code, not framework API.
- The `extern "C"` signature is what Vitis HLS exports as a kernel top function.

### 3.2 Add the HLS kernel implementation

Create `src/kernels/scaleadd_kernel.cpp`:

```cpp
#include "kernels/scaleadd.hpp"

#include "anvil/hls/pack.hpp"

extern "C" void scaleadd(const kernels::ScaleAddPack* a,
                         const kernels::ScaleAddPack* b,
                         kernels::ScaleAddPack* out,
                         float alpha,
                         float beta,
                         int n_packs) {
#pragma HLS INTERFACE m_axi port=a   bundle=gmem0 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=b   bundle=gmem1 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=out bundle=gmem2 offset=slave depth=1024
#pragma HLS INTERFACE s_axilite port=a       bundle=control
#pragma HLS INTERFACE s_axilite port=b       bundle=control
#pragma HLS INTERFACE s_axilite port=out     bundle=control
#pragma HLS INTERFACE s_axilite port=alpha   bundle=control
#pragma HLS INTERFACE s_axilite port=beta    bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control

  for (int i = 0; i < n_packs; ++i) {
#pragma HLS PIPELINE II=1
    kernels::ScaleAddPack pa = a[i];
    kernels::ScaleAddPack pb = b[i];
    kernels::ScaleAddPack po;
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
#pragma HLS UNROLL
      const float av = anvil::hls::GetLane(pa, lane);
      const float bv = anvil::hls::GetLane(pb, lane);
      anvil::hls::SetLane(po, lane, alpha * av + beta * bv);
    }
    out[i] = po;
  }
}
```

Important details:

- `n_packs` is the number of packed words, not scalar elements.
- Memory bundles `gmem0`, `gmem1`, `gmem2` should match `link.cfg` bank bindings.
- The scalar arguments are `s_axilite` control arguments.
- The loop is pipelined at II=1 and lanes are unrolled.

### 3.3 Add a cosim testbench

Create `tests/kernels/scaleadd_cosim_tb.cpp`:

```cpp
#include "kernels/scaleadd.hpp"

#include <cstdio>
#include <vector>

namespace {

constexpr int kPacks = 4;
constexpr int kInterfaceDepthPacks = 1024;

int RunOne() {
  std::vector<kernels::ScaleAddPack> a(kInterfaceDepthPacks);
  std::vector<kernels::ScaleAddPack> b(kInterfaceDepthPacks);
  std::vector<kernels::ScaleAddPack> out(kInterfaceDepthPacks);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
      a[p].Set(lane, static_cast<float>(p * kernels::kScaleAddPackWidth + lane));
      b[p].Set(lane, 10.0f);
      out[p].Set(lane, 0.0f);
    }
  }

  const float alpha = 2.0f;
  const float beta = 3.0f;
  scaleadd(a.data(), b.data(), out.data(), alpha, beta, kPacks);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
      const float av = a[p][lane];
      const float bv = b[p][lane];
      const float expected = alpha * av + beta * bv;
      if (out[p][lane] != expected) {
        std::printf("FAIL p=%d lane=%d got=%g expected=%g\n",
                    p, lane, out[p][lane], expected);
        return 1;
      }
    }
  }
  return 0;
}

}  // namespace

int main() {
  const int rc = RunOne();
  std::printf("scaleadd cosim: %s\n", rc == 0 ? "PASS" : "FAIL");
  return rc;
}
```

This testbench is intentionally simple. It proves the kernel signature, pack type, and lane math before you try board deployment.

### 3.4 Register the kernel in CMake

Edit `src/kernels/CMakeLists.txt`.

Add a new argument list near the existing `saxpy` and `vadd` definitions:

```cmake
set(_scaleadd_kernel_args
  NAME          scaleadd
  TOP           scaleadd
  SOURCES       scaleadd_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})

if(ANVIL_BUILD_TESTS)
  list(APPEND _scaleadd_kernel_args
    TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/scaleadd_cosim_tb.cpp)
endif()

add_anvil_kernel(${_scaleadd_kernel_args})
```

Then decide whether it belongs in the default xclbin. For an accelerator-card target such as U250, you can include it with the other kernels:

```cmake
if(ANVIL_PLATFORM_KIND MATCHES "^(zcu104|zcu102|zcu106|kv260)$")
  set(_saxpy_xclbin_kernels saxpy_xo)
else()
  set(_saxpy_xclbin_kernels saxpy_xo vadd_xo scaleadd_xo)
endif()
```

If you do not want it in the default xclbin, keep it as an independent HLS target and create a separate `add_anvil_xclbin()` block.

### 3.5 Add xclbin connectivity

For `u250`, edit `config/u250/link.cfg`. Add the kernel instance and memory-bank mapping:

```ini
[connectivity]
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1
nk=scaleadd:1:scaleadd_1

sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]

sp=vadd_1.a:DDR[0]
sp=vadd_1.b:DDR[1]
sp=vadd_1.out:DDR[2]

sp=scaleadd_1.a:DDR[0]
sp=scaleadd_1.b:DDR[1]
sp=scaleadd_1.out:DDR[2]

[clock]
freqHz=300000000:saxpy_1
freqHz=300000000:vadd_1
freqHz=300000000:scaleadd_1
```

Rules:

- `nk=<top>:<count>:<cu_name>` creates compute units.
- `sp=<cu_name>.<arg>:<bank>` binds kernel pointer arguments to memory banks.
- The names after the dot must match your kernel function argument names: `a`, `b`, `out`.
- Scalars such as `alpha`, `beta`, and `n_packs` are control-register arguments and do not get `sp=` entries.

For embedded platforms, the exact memory tags depend on the platform. Use the existing target's `link.cfg` as the starting point.

### 3.6 Build and inspect the kernel

Fast configure/build commands:

```bash
make csynth TARGET=u250 KERNEL=scaleadd
make analyze-flow TARGET=u250 KERNEL=scaleadd
make cosim TARGET=u250 KERNEL=scaleadd
make analyze-cosim TARGET=u250 KERNEL=scaleadd
```

Expected results:

- `csynth` produces a `scaleadd_csynth.xml` report under the build tree.
- `analyze-flow` shows II, latency, timing, and resource use.
- `cosim` runs `tests/kernels/scaleadd_cosim_tb.cpp` through Vitis HLS C/RTL cosimulation.
- `analyze-cosim` shows pass/fail and latency extracted from the cosim reports.

If `csynth` passes but `cosim` fails, debug the testbench and ABI first. Do not proceed to xclbin until cosim is clean.

## 4. Add or adapt a host application

Host code lives under `src/host/`. A host app does four jobs:

1. Parse CLI arguments.
2. Load input data or generate synthetic data.
3. Open XRT device, load xclbin, and get a kernel handle.
4. Allocate BOs, copy input, run kernel, copy output back, validate/write output.

### 4.1 Register a new host executable

Create `src/host/run_scaleadd.cpp`, then edit `src/host/CMakeLists.txt`:

```cmake
add_anvil_host(
    NAME run_scaleadd
    SOURCES run_scaleadd.cpp
    LINK_LIBRARIES
        anvil_runtime
        anvil_log
        anvil_cli
        anvil_json
        anvil_compare)

target_include_directories(run_scaleadd PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)
```

Build only this host app:

```bash
make build TARGET=u250 HOST_APP=run_scaleadd
```

This should not run HLS synthesis and should not create a Python environment.

### 4.2 Minimal host skeleton

This is a shortened skeleton. In a real app, copy the dataset loading/writing helpers from `run_saxpy.cpp` or `run_vadd.cpp`.

```cpp
#include <anvil/cli/anvil_cli.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <anvil/runtime/xrt_context.hpp>

#include "kernels/scaleadd.hpp"

#include <algorithm>
#include <filesystem>
#include <numeric>
#include <vector>

namespace fs = std::filesystem;
using anvil::runtime::SyncDirection;
using anvil::runtime::XrtBuffer;
using anvil::runtime::XrtContext;

namespace {
std::size_t RoundUpToPack(std::size_t n) {
  const auto width = static_cast<std::size_t>(kernels::kScaleAddPackWidth);
  return ((n + width - 1) / width) * width;
}
}  // namespace

int main(int argc, char** argv) {
  anvil::log::Init("run_scaleadd");

  anvil::cli::Parser cli("run_scaleadd");
  cli.add_argument("--xclbin").required().help("Path to xclbin containing scaleadd_1");
  cli.add_argument("--n").default_value(1024).scan<'i', int>();
  cli.add_argument("--alpha").default_value(2.0F).scan<'g', float>();
  cli.add_argument("--beta").default_value(3.0F).scan<'g', float>();
  anvil::cli::parse_or_exit(cli, argc, argv);

  const fs::path xclbin = cli.get<std::string>("--xclbin");
  const int n_arg = cli.get<int>("--n");
  const float alpha = cli.get<float>("--alpha");
  const float beta = cli.get<float>("--beta");
  if (n_arg <= 0) return 2;

  const auto n = static_cast<std::size_t>(n_arg);
  const auto padded_n = RoundUpToPack(n);
  const int n_packs = static_cast<int>(padded_n / kernels::kScaleAddPackWidth);

  std::vector<float> a(padded_n, 0.0f), b(padded_n, 0.0f);
  std::iota(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(n), 0.0f);
  std::fill(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(n), 10.0f);

  XrtContext ctx(0, xclbin);
  auto kernel = ctx.GetKernel("scaleadd:{scaleadd_1}");

  XrtBuffer<float> a_buf(ctx, kernel, 0, padded_n);
  XrtBuffer<float> b_buf(ctx, kernel, 1, padded_n);
  XrtBuffer<float> out_buf(ctx, kernel, 2, padded_n);

  std::copy(a.begin(), a.end(), a_buf.host());
  std::copy(b.begin(), b.end(), b_buf.host());
  std::fill(out_buf.host(), out_buf.host() + padded_n, 0.0f);

  a_buf.Sync(SyncDirection::HostToDevice);
  b_buf.Sync(SyncDirection::HostToDevice);

  kernel(a_buf.bo(), b_buf.bo(), out_buf.bo(), alpha, beta, n_packs);

  out_buf.Sync(SyncDirection::DeviceToHost);
  anvil::log::Info("scaleadd finished n={} padded_n={} n_packs={}", n, padded_n, n_packs);
  return 0;
}
```

Common mistakes:

- Passing scalar element count where the kernel expects `n_packs`.
- Using the wrong compute-unit name. It must match `nk=scaleadd:1:scaleadd_1`, so the host uses `scaleadd:{scaleadd_1}`.
- Using BO group indices that do not match memory pointer arguments. For `scaleadd(a, b, out, alpha, beta, n_packs)`, the host-visible memory arguments are `a=0`, `b=1`, `out=2`.

## 5. Add or adapt a golden reference

Golden reference code should be boring, scalar, and easy to review. It is not the place to optimize.

For `scaleadd`, create a header under `src/gold/include/gold/scaleadd_gold.hpp`:

```cpp
#pragma once
#include <span>

namespace gold {

struct ScaleAddConfig {
  float alpha = 1.0f;
  float beta = 1.0f;
};

void scaleadd_gold(std::span<const float> a,
                   std::span<const float> b,
                   std::span<float> out,
                   const ScaleAddConfig& cfg);

}  // namespace gold
```

Create implementation `src/gold/cpp/scaleadd_gold.cpp`:

```cpp
#include "gold/scaleadd_gold.hpp"

#include <stdexcept>

namespace gold {

void scaleadd_gold(std::span<const float> a,
                   std::span<const float> b,
                   std::span<float> out,
                   const ScaleAddConfig& cfg) {
  if (a.size() != b.size() || a.size() != out.size()) {
    throw std::invalid_argument("scaleadd_gold: size mismatch");
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    out[i] = cfg.alpha * a[i] + cfg.beta * b[i];
  }
}

}  // namespace gold
```

Then add the `.cpp` file to `src/gold/CMakeLists.txt`:

```cmake
set(GOLD_SOURCES
  cpp/saxpy_gold.cpp
  cpp/metrics.cpp
  cpp/scaleadd_gold.cpp)
```

Add tests before touching hardware. A small `tests/cpp/test_scaleadd_gold.cpp` should cover:

- empty input
- one element
- non-pack-aligned element count such as 7 or 19
- size mismatch throws
- deterministic repeated calls

## 6. Add or adapt an HLS model

An HLS model is CPU-compiled code that mirrors the kernel structure more closely than the gold reference. For simple kernels you can skip it, but it is useful when:

- the kernel uses packed lanes
- the kernel uses internal streams/dataflow
- you want to compare HLS-model output against gold before Vitis synthesis

Keep HLS model headers in `src/hls_model/include/hls_model/`, not in `include/anvil/`.

A model for `scaleadd` can reuse the same pack type as the kernel:

```cpp
#include "kernels/scaleadd.hpp"
#include "gold/scaleadd_gold.hpp"

#include <span>

namespace hls_model {

void scaleadd_hls_model(std::span<const float> a,
                        std::span<const float> b,
                        std::span<float> out,
                        const gold::ScaleAddConfig& cfg);

}  // namespace hls_model
```

Implementation options:

- If the kernel is simple, call the same scalar loop as gold, then add tests against gold.
- If the kernel uses packed streams, mirror the packed stream pipeline like `src/hls_model/saxpy_hls_model.cpp`.

Register extra source files in `src/hls_model/CMakeLists.txt` if you add them.

## 7. Customize data generation and comparison

The default flow uses `data/<dataset>/` directories. A dataset typically contains:

```text
data/tiny/
├── meta.json
├── x.bin
├── y.bin
├── gold_out.bin
└── xrt_hw_out.bin
```

For a custom kernel, decide the file contract first. For `scaleadd` you might use:

```json
{
  "case_id": "scaleadd_tiny_001",
  "n": 1024,
  "alpha": 2.0,
  "beta": 3.0,
  "a": "a.bin",
  "b": "b.bin",
  "gold": "gold_out.bin",
  "hw": "xrt_hw_out.bin"
}
```

Then update:

| File | What to change |
|---|---|
| `scripts/gen_dataset.py` or `src/apps/gen_dataset.cpp` | Write the new input files and `meta.json` fields |
| `src/gold/**` | Read the same fields and write `gold_out.bin` |
| `src/host/run_<kernel>.cpp` | Read the same inputs and write `xrt_hw_out.bin` |
| `scripts/compare.py` or `tools/hlsflow compare-gold` | Compare the right files and tolerances |

Keep the data contract explicit. If your host writes `scaleadd_hw.bin` but compare expects `xrt_hw_out.bin`, the flow will look broken even if the kernel is correct.

## 8. Add a new target/device

A target starts with a directory under `config/`. Use an existing target close to your board as a template.

### 8.1 Accelerator card target

For an Alveo-like target, create `config/my_u250/anvil.mk`:

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_VITIS_PART     := xcu250-figd2104-2L-e
ANVIL_PLATFORM       ?= /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
ANVIL_PRESET         := my-u250-host
ANVIL_HWEMU_PRESET   := my-u250-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_XCLBIN_MODE    := hw
ANVIL_XRT_LIB        := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo scaleadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim scaleadd_cosim
```

Also add:

- `config/my_u250/link.cfg`
- `config/my_u250/xrt.ini`
- `config/my_u250/README.md`
- optional `config/my_u250/pipeline_demo.cfg`

Then add matching configure/build presets to `CMakePresets.json`. Copy an existing `u250` or `u50` preset and change:

- preset name
- `ANVIL_PLATFORM_KIND`
- `ANVIL_VITIS_PLATFORM`
- target mode (`hw`, `hw_emu`, `sw_emu`)

### 8.2 Embedded target

For a ZynqMP-style target, `config/my_zynq/anvil.mk` typically needs a sysroot:

```make
ANVIL_DEVICE_KIND    := embedded
ANVIL_VITIS_PART     := xczu9eg-ffvb1156-2-e
ANVIL_PLATFORM       ?= $(XILINX_VITIS)/base_platforms/xilinx_zcu102_base_202420_1/xilinx_zcu102_base_202420_1.xpfm
ANVIL_PRESET         := my-zynq-kernel
ANVIL_HOST_PRESET    := my-zynq-host
ANVIL_NEEDS_CROSS    := yes
ANVIL_SYSROOT        ?= $(PETALINUX_SYSROOT)
ANVIL_XCLBIN_MODE    := hw
ANVIL_KERNEL_TARGETS := saxpy_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim
```

Build commands:

```bash
PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux \
  make build-host TARGET=my_zynq HOST_APP=run_saxpy

make csynth TARGET=my_zynq KERNEL=saxpy
make xclbin TARGET=my_zynq
```

If the host compiler cannot find `crtbeginS.o` or `-lgcc`, your sysroot/toolchain pairing is wrong. Use a PetaLinux sysroot that matches your Vitis toolchain and board image.

## 9. Customize platform metadata for reports

`hlsflow` uses `tools/hlsflow/platform_info.py` to display device information such as resource totals, family, memory notes, and default clock. Add your board there when `analyze-flow` shows unknown totals.

A useful metadata entry should include:

- display name
- part number
- family
- default clock
- LUT / FF / DSP / BRAM / URAM / HBM totals if known
- memory topology notes
- whether host is x86_64 or AArch64
- where platform files normally live

This does not affect Vitis builds. It only improves analysis output.

## 10. Customize thresholds and report checks

Use `hlsflow check` when you want a command to fail if reports exceed a budget.

Example direct call:

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check \
  --max-ii 1 \
  --max-lut 200000 \
  --max-dsp 1000 \
  --max-bram 256
```

Typical thresholds:

| Metric | Common rule |
|---|---|
| II | `<= 1` for streaming or packed map loops |
| LUT / FF | Depends on device; use a percentage of available resources |
| DSP | Keep enough headroom for future kernels |
| BRAM/URAM/HBM | Depends on buffering strategy |
| Timing slack | Positive slack is required before hardware confidence |

Do not hide thresholds in random scripts. Keep them visible in Make targets, CI jobs, or documented commands so reviewers know what is being enforced.

## 11. Customize deployment

Deployment is split so you can run only the part you need:

```bash
make deploy-bin TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=192.168.1.10
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.10
make deploy-data TARGET=zcu102 DATASET=tiny BOARD_IP=192.168.1.10
make test-xrt-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=192.168.1.10
```

For a new host app, check these details:

- The binary name under `src/host/CMakeLists.txt` matches `HOST_APP`.
- The xclbin name expected by the Makefile matches the host app (`run_pipeline_demo` uses `pipeline_demo`, others default to `saxpy` unless you customize the Makefile).
- The remote board has XRT installed and sourced.
- Embedded boards have the correct `.xclbin`, host binary, `xrt.ini`, and dataset files in the deployment directory.

If your project uses many host apps and xclbins, consider adding an explicit mapping in the Makefile instead of relying on a two-case `HOST_APP -> XCLBIN_NAME` rule.

## 12. Checklist before you commit a customization

Use this checklist for every new kernel or board:

- [ ] Project-specific headers are under `src/kernels/include/`, `src/hls_model/include/`, or `src/gold/include/`, not `include/anvil/`.
- [ ] Kernel function argument order is documented and matches host BO group indices.
- [ ] `link.cfg` `sp=` names match kernel pointer argument names.
- [ ] `KERNEL=<name>` works for `make csynth`.
- [ ] `KERNEL=<name>` works for `make cosim` if a testbench exists.
- [ ] `HOST_APP=<name>` builds with `make build`.
- [ ] Dataset file names are consistent across generator, gold, host, and compare.
- [ ] `make test` still passes and does not require Vitis/XRT/platform files.
- [ ] `make analyze-flow` shows meaningful platform totals or you added metadata.
- [ ] User docs mention any non-obvious deployment or board setup requirement.

## 13. Troubleshooting common customization failures

### `fatal error: kernels/my_kernel.hpp: No such file or directory`

The target that compiles the file is missing `src/kernels/include` in its include paths.

- Kernels: update `cmake/AnvilKernel.cmake` if the HLS cflags are wrong.
- Host apps: add `target_include_directories(run_my_kernel PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)`.
- Tests: add the include root to `tests/CMakeLists.txt`.

### `cosim is not configured for TARGET=...`

The target's `config/<target>/anvil.mk` does not list the cosim target in `ANVIL_COSIM_TARGETS`, or the kernel was registered without `TESTBENCH`.

### `make xclbin` builds the wrong thing

Check `XCLBIN_NAME` and `add_anvil_xclbin()` registration. The Makefile target builds `$(XCLBIN_NAME)_xclbin` under the current `TARGET`.

### Host app runs but output is wrong

Check in this order:

1. BO group indices in the host app.
2. Kernel function argument order.
3. `link.cfg` `sp=` bindings.
4. Element count vs pack count.
5. Tail-lane padding.
6. Dataset metadata fields.

### Raw CMake gives different pack width than presets

Presets set `ANVIL_PARALLELISM=16`. Raw CMake uses the default from `cmake/ProjectOptions.cmake` unless you pass `-DANVIL_PARALLELISM=...`.

Use presets for the documented flow, or set the option explicitly:

```bash
cmake -S . -B build/custom -DANVIL_PARALLELISM=16
```
