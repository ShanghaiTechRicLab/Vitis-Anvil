# Build Guide

## Requirements

- CMake ≥ 3.21
- Ninja
- C++20 compiler (GCC ≥ 10 or Clang ≥ 13)
- No Vitis, XRT, or Xilinx tools required for Phase 0+1 native builds

## Available presets

| Preset                    | Purpose                                      |
|---------------------------|----------------------------------------------|
| `gold-linux-debug`        | Debug build with gold reference only         |
| `gold-linux-release`      | Release build with gold reference only       |
| `hls-model-linux-debug`   | Debug build with gold + HLS CPU model        |
| `hls-model-linux-release` | Release build with gold + HLS CPU model      |
| `u250-host`         | Phase 2 U250 Vitis HLS csynth preset         |
| `zcu104-host`     | Phase 4 aarch64 host stub (requires SYSROOT) |
| `zcu104-kernel`           | Future embedded/ZCU104 kernel preset; fail-fast until xpfm / Phase 4+ platform work lands |

## Typical workflow

```bash
# Configure + build + test the recommended preset:
rtk cmake --preset hls-model-linux-debug
rtk cmake --build --preset hls-model-linux-debug
rtk ctest --preset hls-model-linux-debug --output-on-failure

# Run the CLI smoke pipeline manually:
rtk ./build/hls-model-linux-debug/src/apps/gen_dataset \
  --manifest tests/data/tiny_case_001.json \
  --data-dir build/hls-model-linux-debug/tests/data
rtk ./build/hls-model-linux-debug/src/apps/run_gold \
  --case tests/data/tiny_case_001.json \
  --data-dir build/hls-model-linux-debug/tests/data
rtk ./build/hls-model-linux-debug/src/apps/compare_gold_hls_model \
  --case tests/data/tiny_case_001.json \
  --data-dir build/hls-model-linux-debug/tests/data
```

## CMake options

- `ANVIL_BUILD_GOLD=ON` — build `anvil_gold`
- `ANVIL_BUILD_HLS_MODEL=ON` — build `anvil_hls_model` (requires gold)
- `ANVIL_BUILD_APPS=ON` — build CLI tools
- `ANVIL_BUILD_TESTS=ON` — build Catch2 tests
- `ANVIL_BUILD_XRT=OFF` — Phase 3 runtime stub
- `ANVIL_BUILD_KERNELS=OFF` — Phase 2 kernel/csynth generation
- `ANVIL_PLATFORM_KIND=native` — `native|u250|alveo_u55c|zcu104|kv260`
- `ANVIL_VITIS_PLATFORM=` — Vitis `.xpfm` path for Phase 2 kernel presets
- `ANVIL_VITIS_TARGET=hw` — Vitis xclbin link target (`hw|hw_emu|sw_emu`); HLS csynth does not pass `--target`
- `ANVIL_PARALLELISM=8` — DataPack lane width; `u250-host` overrides this to `16`
- `ANVIL_MAX_ELEMENTS=131072` — maximum saxpy frame length
- `ANVIL_HLS_STD=c++14` — kernel-synthesis C++ standard (Phase 2)

## Alveo U250 Phase 2 preset

`u250-host` enables the Phase 2 U250 Vitis HLS kernel flow. It builds the
native gold/HLS-model libraries, CLI apps, Catch2 tests, and Vitis kernel
csynthesis (`ANVIL_BUILD_KERNELS=ON`) while keeping the Phase 3 XRT host runtime
off (`ANVIL_BUILD_XRT=OFF`). Source Vitis 2024.2 before configuring so `v++`
and `vitis-run` are on `PATH`:

```bash
source /tools/Xilinx/Vitis/2024.2/settings64.sh
```

The preset defaults `ANVIL_VITIS_PLATFORM` to the lab U250 platform file:

```text
/opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
```

If the platform is installed elsewhere, override it at configure time:

```bash
rtk cmake --preset u250-host -DANVIL_VITIS_PLATFORM=/path/to/xilinx_u250.xpfm
```

Typical Phase 2 usage is configure, build, then run the preset test matrix:

```bash
rtk cmake --preset u250-host
rtk cmake --build --preset u250-host
rtk ctest --preset u250-host --output-on-failure
```

The default build includes `saxpy_xo`, so `rtk cmake --build --preset
u250-host` runs Vitis HLS csynth and packages the `.xo`. The full ctest
matrix includes the native unit/app tests plus Vitis csynth build/check tests
and U250 HLS cosimulation. The current cosim testbench uses `n=65`.

| Target | Trigger | Rough time |
|--------|---------|------------|
| `saxpy_xo` | Default `u250-host` build; also ctest `saxpy_csynth_build` | Minutes |
| `saxpy_cosim` | ctest `saxpy_cosim_vs_gold` builds this target after csynth | Minutes to tens of minutes |
| `saxpy_xclbin` | Manual `rtk cmake --build --preset u250-host --target saxpy_xclbin` | Long-running hardware link; tens of minutes or more |

CTest labels distinguish the Vitis checks from native tests:

- Unlabeled tests (`anvil_tests`, `gen_tiny_dataset_smoke`, `run_gold_smoke`,
  `compare_gold_hls_model_smoke`) are native unit/app smoke tests.
- `csynth` tests build `saxpy_xo` and parse the generated HLS report;
  `saxpy_csynth_build` is also labeled `build` and provides the shared fixture.
- `cosim` tests run `vitis-run --cosim` against the U250 testbench and require
  the csynth fixture.

`ANVIL_VITIS_TARGET` controls only the xclbin link mode (`hw`, `hw_emu`, or
`sw_emu`) and defaults to `hw`; Vitis 2024.2 HLS csynth/cosim do not receive
`--target`. `saxpy_xclbin` is intentionally manual-only: it is not an `ALL`
target, is not registered with ctest, and is long-running. Phase 3 will consume
that artifact when the XRT host runtime is enabled later.

## Exit codes (apps)

- `0` success
- `1` algorithm verdict failed (tolerance)
- `2` input validation failed (bad manifest, file size, n > kMaxElements)
- `3` unexpected internal error
