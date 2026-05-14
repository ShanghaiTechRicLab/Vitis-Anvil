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
| `alveo-u250-host`         | Phase 2 U250 Vitis HLS csynth preset         |
| `zcu104-host-aarch64`     | Phase 4 aarch64 host stub (requires SYSROOT) |
| `zcu104-kernel`           | Phase 2 Vitis kernel stub                    |

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

- `ACCEL_BUILD_GOLD=ON` — build `accel_gold`
- `ACCEL_BUILD_HLS_MODEL=ON` — build `accel_hls_model` (requires gold)
- `ACCEL_BUILD_APPS=ON` — build CLI tools
- `ACCEL_BUILD_TESTS=ON` — build Catch2 tests
- `ACCEL_BUILD_XRT=OFF` — Phase 3 runtime stub
- `ACCEL_BUILD_KERNELS=OFF` — Phase 2 kernel/csynth generation
- `ACCEL_PLATFORM_KIND=native` — `native|alveo_u250|alveo_u55c|zcu104|kv260`
- `ACCEL_VITIS_PLATFORM=` — Vitis `.xpfm` path for Phase 2 kernel presets
- `ACCEL_VITIS_TARGET=hw` — Vitis xclbin link target (`hw|hw_emu|sw_emu`); HLS csynth does not pass `--target`
- `ACCEL_PARALLELISM=8` — reserved for Phase 2 (DataPack lane width)
- `ACCEL_MAX_ELEMENTS=131072` — maximum saxpy frame length
- `ACCEL_HLS_STD=c++14` — kernel-synthesis C++ standard (Phase 2)

## Alveo U250 Phase 2 preset

`alveo-u250-host` enables the Phase 2 Vitis HLS kernel flow with
`ACCEL_BUILD_KERNELS=ON` and `ACCEL_BUILD_XRT=OFF`; it does not build the Phase
3 XRT host runtime. `saxpy_xo` is part of the default build, while the U250
`saxpy_xclbin` link is an explicit, long-running target and is not part of
`ALL` or ctest:

```bash
rtk cmake --build --preset alveo-u250-host --target saxpy_xclbin
```

`ACCEL_VITIS_TARGET` controls the xclbin link mode (`hw`, `hw_emu`, or
`sw_emu`) and defaults to `hw`. The preset uses the repository default U250
platform path:

```text
/opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
```

If the platform is installed elsewhere, override it at configure time:

```bash
rtk cmake --preset alveo-u250-host -DACCEL_VITIS_PLATFORM=/path/to/xilinx_u250.xpfm
```

## Exit codes (apps)

- `0` success
- `1` algorithm verdict failed (tolerance)
- `2` input validation failed (bad manifest, file size, n > kMaxElements)
- `3` unexpected internal error
