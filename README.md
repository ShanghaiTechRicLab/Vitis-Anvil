# Vitis-Anvil

A CMake + Vitis HLS + XRT template for heterogeneous accelerator projects.
Use this as the starting point for FPGA/HLS acceleration work targeting
Alveo PCIe cards or Zynq UltraScale+ / Kria / Versal embedded SoCs.

## Status

**Phase 0+1 implemented** — CMake skeleton, vendored third-party libs,
gold reference (`saxpy_gold`), HLS-friendly CPU model (`saxpy_hls_model`),
Catch2 test suite, and CLI apps (`gen_dataset`, `run_gold`,
`compare_gold_hls_model`).

Phase 2–6 (Vitis kernel synthesis, XRT runtime, embedded deployment,
Python `hlsflow` automation) are scaffolded as stubs and will be filled
in by future plans.

## Requirements

- CMake ≥ 3.21
- C++20 compiler (GCC ≥ 10 or Clang ≥ 13)
- Ninja (recommended)
- No Vitis / XRT / Xilinx tools required for Phase 0+1

## Quickstart

```bash
rtk cmake --preset hls-model-linux-debug
rtk cmake --build --preset hls-model-linux-debug
rtk ctest --preset hls-model-linux-debug --output-on-failure
```

See `docs/build.md` for all presets and `docs/superpowers/specs/2026-05-14-vitis-anvil-phase01-design.md`
for the full design.

## Layout

- `include/accel/` — public headers
- `src/gold/` — algorithmic reference (`saxpy_gold`)
- `src/hls_model/` — HLS-friendly CPU model (`saxpy_hls_model`)
- `src/apps/` — CLI tools (`gen_dataset`, `run_gold`, `compare_gold_hls_model`)
- `tests/` — Catch2 unit tests
- `third_party/` — vendored dependencies (see `third_party/THIRD_PARTY_NOTICES.md`)
- `cmake/` — CMake helpers and toolchain files
- `docs/` — design and build documentation

## License

See `LICENSE`. Vendored third-party code retains its upstream licenses
under `third_party/<lib>/LICENSE*`.
