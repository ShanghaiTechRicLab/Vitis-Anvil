# Vitis-Anvil

A CMake + Vitis HLS + XRT template for heterogeneous accelerator projects.
Use this as the starting point for FPGA/HLS acceleration work targeting
Alveo PCIe cards or Zynq UltraScale+ / Kria / Versal embedded SoCs.

## Status

**Phase 0+1 implemented** — CMake skeleton, vendored third-party libs,
gold reference (`saxpy_gold`), HLS-friendly CPU model (`saxpy_hls_model`),
Catch2 test suite, and CLI apps (`gen_dataset`, `run_gold`,
`compare_gold_hls_model`).

**Phase 2 partially implemented** — Vitis HLS `saxpy_xo` csynth/cosim targets
and an explicit U250 `saxpy_xclbin` link target are registered. The xclbin link
is long-running, is not part of `ALL`, and is not driven by ctest.

Phase 3–6 (XRT runtime, embedded deployment, Python `hlsflow` automation)
remain scaffolded for future plans.

## Requirements

- CMake ≥ 3.21
- C++20 compiler (GCC ≥ 10 or Clang ≥ 13)
- Ninja (recommended)
- No Vitis / XRT / Xilinx tools required for Phase 0+1 native builds
- Vitis and a platform `.xpfm` are required only when `ACCEL_BUILD_KERNELS=ON`

## Quickstart

```bash
rtk cmake --preset hls-model-linux-debug
rtk cmake --build --preset hls-model-linux-debug
rtk ctest --preset hls-model-linux-debug --output-on-failure
```

See `docs/build.md` for presets and Phase 0+1 build/test usage. Future phase
design notes will be added to tracked docs as those phases land.

## Layout

- `include/accel/` — public headers
- `src/gold/` — algorithmic reference (`saxpy_gold`)
- `src/hls_model/` — HLS-friendly CPU model (`saxpy_hls_model`)
- `src/apps/` — CLI tools (`gen_dataset`, `run_gold`, `compare_gold_hls_model`)
- `src/kernels/` — Vitis HLS saxpy kernel and explicit U250 xclbin target
- `tests/` — Catch2 unit tests
- `third_party/` — vendored dependencies (see `third_party/THIRD_PARTY_NOTICES.md`)
- `cmake/` — CMake helpers and toolchain files
- `docs/` — design and build documentation

## License

See `LICENSE`. Vendored third-party code retains its upstream licenses
under `third_party/<lib>/LICENSE*`.
