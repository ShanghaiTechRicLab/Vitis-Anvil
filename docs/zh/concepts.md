# 基本概念：整个流程到底在做什么

这篇假设你只知道一个大概：FPGA 可以用 HLS 从 C/C++ 生成硬件。Vitis-Anvil 做的是把这个想法周围需要的工程流程补齐。

## 一句话版本

一个 FPGA 应用通常有两个程序：

1. **Kernel** — 会被综合成 FPGA 硬件的函数。
2. **Host app** — 跑在 CPU 上的程序，负责加载 FPGA 二进制、分配 buffer、拷贝数据、启动 kernel、读回结果。

中间还有一个 FPGA 二进制文件：

- **xclbin** — Vitis 生成的文件，里面包含 kernel 硬件和连接信息，运行时由 XRT 加载。

正常开发流程采用 model-first 顺序：

```text
gold -> hls_model -> csynth -> cosim -> xclbin -> swemu/hwemu/qemu/hw
```

Vitis-Anvil 给每一步固定了目录和 Make 命令。

## 关键名词

### Kernel

Kernel 是 Vitis HLS 会转成硬件的 C/C++ 函数。本项目里的 kernel 放在：

```text
src/kernels/*.cpp
src/kernels/include/kernels/*.hpp
```

头文件很重要，因为同一个函数签名要给这些地方共用：

- kernel 实现
- cosim testbench
- 有时 host app 也要用同样的 pack 宽度或 ABI 常量

### Host app

Host app 跑在 CPU 上，不在 FPGA 上。它用 XRT 做这些事：

1. 打开 FPGA 设备
2. 加载 xclbin
3. 分配输入/输出 buffer
4. 把数据拷到设备
5. 启动 kernel
6. 把输出拷回来

Host app 放在：

```text
src/host/*.cpp
```

已有例子包括 `run_saxpy`、`run_vadd`、`run_pipeline_demo`。

### Gold reference

Gold reference 是最简单的 CPU 正确性实现，也就是“真值”。它应该容易读，不追求性能。

Gold 代码放在：

```text
src/gold/**
```

它回答的问题是：“FPGA 正确输出应该是什么？”

### HLS model

HLS model 是 CPU 上编译运行的模型，但结构更接近 HLS kernel。你的 kernel 如果用了 pack、stream、dataflow，HLS model 会很有用。

HLS model 放在：

```text
src/hls_model/**
```

它用来在跑 Vitis 综合之前先抓 pack、stream、dataflow 和尾部元素处理错误。HLS 模型不能证明 timing、资源、Vitis link、XRT 或真实板卡行为。

### 内核核心（kernel core）

内核核心是项目自己的 HLS 兼容代码，同时被 HLS 模型和 Vitis top 复用。`saxpy` 的共享内核核心在：

```text
src/kernels/include/kernels/saxpy_core.hpp
```

它放 packed operation 和 load/compute/store 阶段 helper；不放 host 代码、XRT 代码或板卡配置。

### Vitis top

Vitis top 是 `extern "C"` 包装函数，Vitis HLS 会把它变成硬件 kernel。它拥有 kernel ABI 和 interface pragmas。`saxpy` 的 top 在：

```text
src/kernels/saxpy_kernel.cpp
```

top 会调用共享内核核心，但把 `#pragma HLS INTERFACE` 和显式 dataflow 区域这类硬件边界细节留在 top-level kernel 文件中。

### Saxpy 文件地图

Demo `saxpy` 按职责拆成这些文件：

```text
src/gold/include/gold/saxpy_gold.hpp
src/kernels/include/kernels/saxpy_core.hpp
src/kernels/saxpy_kernel.cpp
src/hls_model/include/hls_model/saxpy_hls_model.hpp
src/hls_model/saxpy_hls_model.cpp
src/host/run_saxpy.cpp
```

- `saxpy_gold.hpp` 声明普通 CPU 数学真值。
- `saxpy_core.hpp` 放 HLS 兼容的 kernel core，由模型和 top 共享。
- `saxpy_kernel.cpp` 是 Vitis top：ABI、HLS pragmas、显式 dataflow 调用。
- `saxpy_hls_model.hpp/.cpp` 把 CPU scalar span 打包成 HLS 形状的数据，调用共享核心，再解包结果。
- `run_saxpy.cpp` 是 XRT host app，负责在卡/板上运行 xclbin。

这次 first-class HLS 模型主要覆盖 m_axi 风格的 packed kernels。现有 stream/k2k kernels 仍然支持，但 first-class stream HLS models 是后续设计。

### Dataset

Dataset 是输入和参考输出目录。通常长这样：

```text
data/<name>/meta.json
data/<name>/x.bin
data/<name>/y.bin
data/<name>/gold_out.bin
```

运行输出是独立产物，放在
`runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/out.bin`，不会写回
`data/`。具体文件取决于你的 kernel。关键是 generator、gold、host app、compare
工具必须对文件名和含义达成一致。

### csynth

`csynth` 是 HLS C synthesis。Vitis 读取 kernel C++，生成报告和 kernel object。

它回答这些问题：

- Vitis 能不能接受我的 HLS 代码？
- 循环 II 是多少？
- 估算用了多少 LUT/FF/DSP/BRAM？
- 估算时钟是多少？

命令：

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

### cosim

`cosim` 是 C/RTL 协同仿真。Vitis 用 C++ testbench 去跑生成的 RTL，检查 RTL 行为是否和 C++ kernel 一致。

它不是 host app 运行，也不会加载 xclbin。它仍然是 kernel 级别的测试。

命令：

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

### xclbin

xclbin 是 XRT 加载的 FPGA 二进制。它在 HLS 综合之后由 Vitis linker 生成。

linker 会使用：

- 综合得到的 kernel object
- platform `.xpfm`
- `link.cfg` 连接规则

命令：

```bash
make xclbin TARGET=u250
```

### platform / xpfm

Platform 文件（`.xpfm`）告诉 Vitis 目标板卡/加速卡是什么：器件型号、时钟、内存接口、shell、支持的 flow 等。

每个 target config 会指向一个 platform：

```text
config/u250/anvil.mk
config/zcu102/anvil.mk
```

### XRT

XRT 是 host app 用来和 FPGA 通信的运行时库。如果 host app 编译失败，说缺 XRT 头文件或库，这不一定是 HLS kernel 错了，而是 CPU 侧运行时环境有问题。

### TARGET, KERNEL, HOST_APP, DATASET

这些 Make 变量选择的是不同东西：

| 变量 | 选择什么 | 例子 |
|---|---|---|
| `TARGET` | 板卡/platform 配置 | `u250`, `zcu102` |
| `KERNEL` | HLS kernel 目标 | `saxpy`, `vadd`, `all` |
| `HOST_APP` | CPU host 可执行程序 | `run_saxpy` |
| `DATASET` | 数据目录 | `tiny` |

不要混用。`HOST_APP` 不选择 cosim；`KERNEL` 才选择 cosim。

## 下一步读什么

- 第一次用：读 [快速开始](get_started.md)。
- 想加自己的算法：读 [自定义指南](customization.md)。
- 用加速卡：读 [加速卡流程](accelerator_flow.md)。
- 用嵌入式板：读 [Embedded 流程](embedded_flow.md)。
