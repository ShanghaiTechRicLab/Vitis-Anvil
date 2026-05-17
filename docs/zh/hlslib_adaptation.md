# hlslib 适配模式

这篇解释 Vitis-Anvil 如何使用 hlslib。你不需要提前了解 hlslib；关键是 hlslib 提供了适合 HLS 的 packed data 和 stream 类型，Anvil 只封装模板项目需要的一小部分。

## 1. 为什么要用 hlslib？

FPGA kernel 常常每周期处理多个值。普通 `float` 是一个值，packed vector 可以在一个 word 里放 4、8、16 个 float。hlslib 提供 `DataPack<T, N>` 来表达这个模式。

FPGA kernel 也常用 stream 连接多个阶段。hlslib 提供适合仿真的 `Stream<T, Depth>` 和 dataflow macros。

Anvil 加了一层薄封装，让用户代码风格统一：

```cpp
anvil::hls::Pack<float, 16>
anvil::hls::Stream<MyPack, 32>
anvil::hls::GetLane(pack, lane)
anvil::hls::SetLane(pack, lane, value)
ANVIL_DATAFLOW_FUNCTION(...)
```

## 2. 哪些是框架代码，哪些是项目代码？

框架拥有，默认不要改：

```text
include/anvil/**
src/anvil/**
```

项目拥有，预期会改：

```text
src/kernels/**
src/kernels/include/kernels/**
src/hls_model/**
src/host/**
src/gold/**
config/**
```

不要把你的 kernel ABI 类型放到 `include/anvil/`。应该放到 `src/kernels/include/kernels/`。

## 3. 辅助头文件

| Header | 提供什么 | 什么时候用 |
|---|---|---|
| `anvil/hls/pack.hpp` | `Pack`, `PackTraits`, lane get/set | packed memory 或 vector lane |
| `anvil/hls/stream.hpp` | `Stream<T, Depth>` | kernel 内部 dataflow stream |
| `anvil/hls/dataflow.hpp` | dataflow macros | load/compute/store 多阶段 kernel |
| `anvil/hls/packed_ops.hpp` | load/store/map helpers | 常见 packed loop |
| `anvil/hls/axis.hpp` | `ReadAxis`, `WriteAxis` | 外部 `hls::stream` 端口 |

## 4. Pack 例子

```cpp
typedef anvil::hls::Pack<float, 16> Float16;

Float16 p;
for (int lane = 0; lane < 16; ++lane) {
  anvil::hls::SetLane(p, lane, static_cast<float>(lane));
}
float x = anvil::hls::GetLane(p, 3);
```

尽量用 lane helper，而不是直接 `p[lane]`。这样项目风格统一，后续也更容易改。

## 5. Stream/dataflow 例子

常见 kernel 结构：

```text
从内存 Load → Compute → Store 回内存
```

dataflow 可以让这些阶段重叠执行。

规则：stream 变量直接传，不要包 `std::ref`。

```cpp
ANVIL_DATAFLOW_INIT();
ANVIL_DATAFLOW_FUNCTION(Load, input, s_in, n);
ANVIL_DATAFLOW_FUNCTION(Compute, s_in, s_out, n);
ANVIL_DATAFLOW_FUNCTION(Store, s_out, output, n);
ANVIL_DATAFLOW_FINALIZE();
```

为什么不能 `std::ref`？hlslib simulation 已经会根据被调用函数签名保留引用参数。调用点再包 `std::ref` 可能 double-wrap，导致编译失败。

## 6. 和 HLS 模型共享内核核心

项目专用的 HLS 兼容 helper 应该放在 `src/kernels/include/kernels/**`，不要放进 `include/anvil/**`。例如 `src/kernels/include/kernels/saxpy_core.hpp` 拥有 `SaxpyOp` 和 load/compute/store helpers，并同时被这些文件使用：

- `src/kernels/saxpy_kernel.cpp`：带 interface pragmas 和显式 dataflow 区域的 Vitis top
- `src/hls_model/saxpy_hls_model.cpp`：CPU adapter，负责把 scalar spans 打包、调用同一套内核核心 helper、再解包结果

这个拆分服务于 model-first 阶梯：

```text
gold -> hls_model -> csynth -> cosim -> xclbin -> swemu/hwemu/qemu/hw
```

除非综合报告证明硬件不变，不要为了方便把 Vitis dataflow 区域藏进 wrapper。把 `ANVIL_DATAFLOW_INIT`、`ANVIL_DATAFLOW_FUNCTION` 和 `ANVIL_DATAFLOW_FINALIZE` 显式留在 Vitis top 里，更容易审查硬件边界。

## 7. 外部 AXI stream

会变成 AXI stream 的 kernel top-level 端口，继续用 Vitis `hls::stream<...>`。函数内部可以用 Anvil helper：

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

除非你清楚 Vitis link 的影响，否则不要把外部 stream 端口改成 hlslib stream。

First-class HLS 模型目前主要面向 m_axi 风格 packed kernels。现有 stream/k2k kernels 仍由 Vitis flow 支持，但 stream HLS model 设计属于单独工作。

## 8. Pack 宽度和 ABI

Pack 宽度是 kernel ABI 的一部分。如果 host 和 kernel 不一致，buffer padding 会错，输出也会错。

Demo kernel 的宽度在：

```text
src/kernels/include/kernels/kernel_types.hpp
```

`saxpy` 跟随 `ANVIL_PARALLELISM`。`vadd` 和 `pipeline_demo` 使用固定 demo 宽度。你加自己的 kernel 时，在自己的 ABI header 中定义宽度，并在 host、kernel、tests 中使用同一个常量。

## 9. 下一层：HLS 微架构原语

hlslib wrapper 之上，可以使用 [HLS 微架构原语](hls_primitives.md) 来组织 tile
buffer、burst load/store、定点归约、小规模排序，以及 double-buffered
load-compute-store 骨架。
