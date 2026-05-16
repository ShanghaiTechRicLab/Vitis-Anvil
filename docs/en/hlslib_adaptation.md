# hlslib adaptation pattern

Vitis-Anvil uses hlslib for the reusable HLS building blocks, but keeps demo and user code outside the framework tree.

## Ownership boundary

Framework-owned, default not editable:

- `include/anvil/**` — reusable public Anvil headers
- `src/anvil/**` — reusable Anvil implementation

Project/user-owned, expected to change:

- `src/kernels/**` — synthesizable kernels
- `src/kernels/include/kernels/**` — kernel ABI types and declarations
- `src/hls_model/**` — CPU/HLS models for your kernels
- `src/host/**` — XRT host apps
- `src/apps/**` — CPU utility apps
- `config/**` — board/platform/link configuration

Do not put project-specific kernels such as `saxpy` or `vadd` under `include/anvil/**`. That tree is installed/exported as the framework API.

## Generic Anvil HLS helpers

The generic helper headers live under `include/anvil/hls/`:

- `pack.hpp` — `anvil::hls::Pack<T, N>`, `PackTraits`, `GetLane`, `SetLane`
- `stream.hpp` — `anvil::hls::Stream<T, Depth>` and default depths
- `dataflow.hpp` — `ANVIL_DATAFLOW_*` wrappers
- `packed_ops.hpp` — load/store/map helpers for packed streams and memory
- `axis.hpp` — small `ReadAxis`/`WriteAxis` wrappers for `hls::stream` style ports
- `hls_aliases.hpp` — compatibility header for older examples

The helpers are C++14-clean because Vitis HLS compiles kernels with `ANVIL_HLS_STD`.

## Pack widths

Demo kernel pack types live in `src/kernels/include/kernels/kernel_types.hpp`.

- `kernels::kSaxpyPackWidth` follows `anvil::config::kParallelism` / `ANVIL_PARALLELISM`.
- `kernels::kVaddPackWidth` and `kernels::kPipelinePackWidth` remain fixed at 16 for the demos.
- Presets set `ANVIL_PARALLELISM=16`; raw CMake defaults to 8, so raw builds use an 8-lane saxpy ABI consistently across host/model/kernel.

Use the named constants in host code, kernels, and testbenches. Do not duplicate a literal `16` for saxpy.

## Dataflow rule

Pass hlslib streams directly to dataflow functions:

```cpp
ANVIL_DATAFLOW_FUNCTION(Compute, sx, sy, so, a, n_pack);
```

Do not wrap streams in `std::ref`. hlslib v1.4.6 applies reference preservation from the callee function signature during simulation; wrapping at the call site can double-wrap and fail to bind.

## Adapting your own kernel

1. Add ABI declarations and pack types under `src/kernels/include/kernels/`.
2. Add synthesizable code under `src/kernels/`.
3. Reuse `anvil/hls/pack.hpp`, `stream.hpp`, `dataflow.hpp`, and `packed_ops.hpp` for internal dataflow.
4. Keep external AXI stream kernel ports as `hls::stream<...>` for Vitis link compatibility; use `ReadAxis`/`WriteAxis` inside the implementation.
5. Register kernels in `src/kernels/CMakeLists.txt` with `add_anvil_kernel()`.
6. Put host-side models under `src/hls_model/`, not under `include/anvil/`.
