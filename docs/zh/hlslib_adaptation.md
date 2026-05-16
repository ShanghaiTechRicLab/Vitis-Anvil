# hlslib 适配模式

Vitis-Anvil 用 hlslib 提供可复用的 HLS 基础设施，但 demo 和用户代码必须放在框架树之外。

## 归属边界

框架拥有，默认不要改：

- `include/anvil/**` — Anvil 公共框架头文件
- `src/anvil/**` — Anvil 框架实现

项目/用户拥有，预期会改：

- `src/kernels/**` — 可综合 kernel
- `src/kernels/include/kernels/**` — kernel ABI 类型和声明
- `src/hls_model/**` — 你的 kernel 的 CPU/HLS 模型
- `src/host/**` — XRT host app
- `src/apps/**` — CPU 工具程序
- `config/**` — 板卡、platform、link 配置

不要把 `saxpy`、`vadd` 这类项目专用 kernel 放到 `include/anvil/**`。这个目录会作为框架 API 安装/导出。

## 通用 Anvil HLS 辅助

通用辅助头文件位于 `include/anvil/hls/`：

- `pack.hpp` — `anvil::hls::Pack<T, N>`、`PackTraits`、`GetLane`、`SetLane`
- `stream.hpp` — `anvil::hls::Stream<T, Depth>` 和默认深度
- `dataflow.hpp` — `ANVIL_DATAFLOW_*` 封装
- `packed_ops.hpp` — packed stream/memory 的 load、store、map 辅助
- `axis.hpp` — 面向 `hls::stream` 风格端口的 `ReadAxis` / `WriteAxis`
- `hls_aliases.hpp` — 兼容旧示例的头文件

这些辅助保持 C++14 clean，因为 Vitis HLS 会用 `ANVIL_HLS_STD` 编译 kernel。

## Pack 宽度

Demo kernel 的 pack 类型位于 `src/kernels/include/kernels/kernel_types.hpp`。

- `kernels::kSaxpyPackWidth` 跟随 `anvil::config::kParallelism` / `ANVIL_PARALLELISM`。
- `kernels::kVaddPackWidth` 和 `kernels::kPipelinePackWidth` 在 demo 中固定为 16。
- Presets 设置 `ANVIL_PARALLELISM=16`；直接 raw CMake 默认是 8，因此 raw 构建会在 host/model/kernel 三侧一致使用 8-lane saxpy ABI。

Host、kernel、testbench 都使用命名常量。不要为 saxpy 再手写一个字面量 `16`。

## Dataflow 规则

把 hlslib stream 变量直接传给 dataflow function：

```cpp
ANVIL_DATAFLOW_FUNCTION(Compute, sx, sy, so, a, n_pack);
```

不要对 stream 使用 `std::ref`。hlslib v1.4.6 在 simulation 中会根据被调用函数签名处理引用参数；调用点再包一层 `std::ref` 可能造成 double-wrap，导致绑定失败。

## 适配你自己的 kernel

1. 在 `src/kernels/include/kernels/` 放 ABI 声明和 pack 类型。
2. 在 `src/kernels/` 放可综合实现。
3. 内部 dataflow 复用 `anvil/hls/pack.hpp`、`stream.hpp`、`dataflow.hpp`、`packed_ops.hpp`。
4. 外部 AXI stream kernel 端口继续用 `hls::stream<...>` 以保持 Vitis link 兼容；实现内部用 `ReadAxis` / `WriteAxis`。
5. 在 `src/kernels/CMakeLists.txt` 中用 `add_anvil_kernel()` 注册 kernel。
6. Host 侧模型放 `src/hls_model/`，不要放 `include/anvil/`。
