# hlslib adaptation pattern

This page explains how Vitis-Anvil uses hlslib. You do not need to know hlslib before reading this; the important idea is that hlslib provides convenient C++ types for packed data and streams, and Anvil wraps the small subset used by the template.

## 1. Why use hlslib at all?

FPGA kernels often process multiple values per cycle. A normal `float` is one value. A packed vector can hold 4, 8, or 16 floats in one word. hlslib provides `DataPack<T, N>` for this pattern.

FPGA kernels also often connect stages with streams. hlslib provides a simulation-friendly `Stream<T, Depth>` and dataflow macros.

Anvil adds thin wrappers so user code has one consistent style:

```cpp
anvil::hls::Pack<float, 16>
anvil::hls::Stream<MyPack, 32>
anvil::hls::GetLane(pack, lane)
anvil::hls::SetLane(pack, lane, value)
ANVIL_DATAFLOW_FUNCTION(...)
```

## 2. What is framework code and what is project code?

Framework-owned, default not editable:

```text
include/anvil/**
src/anvil/**
```

Project-owned, expected to change:

```text
src/kernels/**
src/kernels/include/kernels/**
src/hls_model/**
src/host/**
src/gold/**
config/**
```

Do not put your kernel ABI types under `include/anvil/`. Put them under `src/kernels/include/kernels/`.

## 3. The helper headers

| Header | What it gives you | When to use it |
|---|---|---|
| `anvil/hls/pack.hpp` | `Pack`, `PackTraits`, lane get/set | packed memory or vector lanes |
| `anvil/hls/stream.hpp` | `Stream<T, Depth>` | internal dataflow streams |
| `anvil/hls/dataflow.hpp` | dataflow macros | multi-stage load/compute/store kernels |
| `anvil/hls/packed_ops.hpp` | load/store/map helpers | common packed loops |
| `anvil/hls/axis.hpp` | `ReadAxis`, `WriteAxis` | external `hls::stream` ports |

## 4. Pack example

```cpp
typedef anvil::hls::Pack<float, 16> Float16;

Float16 p;
for (int lane = 0; lane < 16; ++lane) {
  anvil::hls::SetLane(p, lane, static_cast<float>(lane));
}
float x = anvil::hls::GetLane(p, 3);
```

Use lane helpers instead of direct `p[lane]` when possible. It keeps the project style consistent and makes future changes easier.

## 5. Stream/dataflow example

A common kernel structure is:

```text
Load from memory → Compute → Store to memory
```

With dataflow, those stages can overlap.

Rule: pass stream variables directly. Do not wrap them in `std::ref`.

```cpp
ANVIL_DATAFLOW_INIT();
ANVIL_DATAFLOW_FUNCTION(Load, input, s_in, n);
ANVIL_DATAFLOW_FUNCTION(Compute, s_in, s_out, n);
ANVIL_DATAFLOW_FUNCTION(Store, s_out, output, n);
ANVIL_DATAFLOW_FINALIZE();
```

Why no `std::ref`? hlslib simulation already preserves reference parameters based on the called function signature. Adding `std::ref` at the call site can double-wrap the stream and break compilation.

## 6. Sharing kernel core with the HLS model

Project-specific HLS-compatible helpers belong under `src/kernels/include/kernels/**`, not under `include/anvil/**`. For example, `src/kernels/include/kernels/saxpy_core.hpp` owns the `SaxpyOp` and the load/compute/store helpers used by both:

- `src/kernels/saxpy_kernel.cpp`, the Vitis top with interface pragmas and an explicit dataflow region
- `src/hls_model/saxpy_hls_model.cpp`, the CPU adapter that packs scalar spans, calls the same kernel core helpers, and unpacks results

That split supports the model-first ladder:

```text
gold -> hls_model -> csynth -> cosim -> xclbin -> host
```

Do not hide a Vitis dataflow region in a convenience wrapper unless synthesis reports prove the generated hardware is unchanged. Keeping `ANVIL_DATAFLOW_INIT`, `ANVIL_DATAFLOW_FUNCTION`, and `ANVIL_DATAFLOW_FINALIZE` explicit in the Vitis top makes the hardware boundary easier to review.

## 7. External AXI streams

For kernel ports that become AXI streams, keep using Vitis `hls::stream<...>` in the top-level function signature. Use Anvil helpers inside the function:

```cpp
extern "C" void my_stream_kernel(hls::stream<MyPack>& in,
                                 hls::stream<MyPack>& out,
                                 int n) {
  for (int i = 0; i < n; ++i) {
    MyPack p = anvil::hls::ReadAxis(in);
    anvil::hls::WriteAxis(out, p);
  }
}
```

Do not change external stream ports to hlslib streams unless you know the Vitis link implications.

First-class HLS models currently focus on m_axi-style packed kernels. Existing stream/k2k kernels remain supported in the Vitis flow, but stream HLS model design is separate work.

## 8. Pack width and ABI

Pack width is part of the kernel ABI. If host and kernel disagree, buffers will be padded incorrectly and output will be wrong.

For demo kernels, widths live in:

```text
src/kernels/include/kernels/kernel_types.hpp
```

`saxpy` follows `ANVIL_PARALLELISM`. `vadd` and `pipeline_demo` use fixed demo widths. If you add your own kernel, define its width in its own ABI header and use that same constant in host, kernel, and tests.
