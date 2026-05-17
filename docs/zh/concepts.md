# 基本概念：整个流程到底在做什么

这篇假设你只知道一个大概：FPGA 可以用 HLS 从 C/C++ 生成硬件。Vitis-Anvil 做的是把这个想法周围的工程流程补齐。读完这篇再跑第一条命令。大神可以跳读，专找不熟悉的术语。

## 一句话版本

一个 FPGA 加速项目有三样东西：跑在 CPU 上的程序（host app）、跑在 FPGA 上的程序（kernel），以及把 C++ 编译成 FPGA 硬件的工具链（Vitis）。Vitis-Anvil 给这三样提供了项目结构、构建规则和测试梯。

---

## 两个程序，一个二进制文件

每个 FPGA 加速器同时跑着两个程序：

**Kernel** — Vitis HLS 编译成 FPGA 硬件的 C++ 函数，跑在 FPGA fabric 上。

**Host app** — 普通 C++，跑在 CPU 上。它加载 FPGA 二进制文件到设备，分配内存 buffer，把数据拷到 FPGA，启动 kernel，把结果读回来。

中间是 **xclbin**：Vitis 生成的 FPGA 二进制文件，由 host app 通过 XRT 在运行时加载。

```text
[CPU：host app]  ──XRT──→  [xclbin 加载到 FPGA]  ──片上内存──→  结果
```

开发流程按 model-first 顺序，优先用便宜的检查抓 bug，再用慢的 FPGA 工具：

```text
gold → hls_model → csynth → cosim → xclbin → swemu/hwemu/qemu/hw
```

---

## 四大 Make 变量

大多数命令接受这四个变量。搞清楚这四个，再谈别的。

| 变量 | 选择什么 | 示例值 |
|---|---|---|
| `TARGET` | 板卡/platform 配置（`config/<target>/anvil.mk`） | `u250`, `u55c`, `zcu102`, `kv260` |
| `KERNEL` | HLS kernel 目标名 | `saxpy`, `vadd`, `pipeline_demo`, `all` |
| `HOST_APP` | 要构建或运行的 CPU host 可执行程序 | `run_saxpy`, `run_vadd` |
| `DATASET` | `data/` 下的输入数据集目录名 | `tiny`, `my_dataset` |

例子：

```bash
make csynth TARGET=u250 KERNEL=saxpy
make hw     TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

不要混用。`HOST_APP` 不决定综合哪个 kernel；`KERNEL` 才决定。

---

## MODE：为哪个环境构建？

`MODE` 是第五个变量，控制 Vitis 构建出什么、XRT 在哪个环境里跑。

| MODE | 环境 | 需要 Vitis？ | 需要 XRT？ | 需要物理板卡？ |
|---|---|:---:|:---:|:---:|
| `hw` | 真实 FPGA 硬件 | 构建 xclbin 需要 | 运行时需要 | 是 |
| `hw_emu` | Hardware emulation（RTL 仿真） | 是 | 是 | 否 |
| `sw_emu` | Software emulation（快速 C 模型） | 是 | 是 | 否 |
| `qemu` | 嵌入式 QEMU（ZynqMP/MPSoC） | 是 | 板上需要 | 否 |

`MODE` 默认是 `hw`。要跑 emulation 时明确传：

```bash
make csynth   TARGET=u250 MODE=hw_emu KERNEL=saxpy
make xclbin   TARGET=u250 MODE=hw_emu HOST_APP=run_saxpy
make emconfig TARGET=u250 MODE=sw_emu
```

run 命令按名字固定了 mode，不需要额外传 MODE：

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # 始终 MODE=sw_emu
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # 始终 MODE=hw_emu
make hw    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # 始终 MODE=hw
make qemu  TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny # 始终 MODE=qemu
```

CPU-only 阶段（`test`、`gold`、HLS 模型）完全忽略 `TARGET` 和 `MODE`。

### sw_emu vs hw_emu：用哪个？

| | sw_emu | hw_emu |
|---|---|---|
| 跑的是什么 | 原始 C++ kernel 编译成 x86_64 的快速行为模型 | 综合后的硬件 RTL 仿真（Vivado xsim） |
| 速度 | 快：几分钟 | 慢：大数据可能跑几小时 |
| 能抓住什么 | Host/XRT API bug、buffer 设置、参数传递 | RTL 正确性、接口 timing、内存协议 |
| 需要先跑 csynth？ | 否（跳过综合） | 是（必须先综合 kernel） |
| 数据集大小 | 可以用正常大小 | 用非常小的数据；大数据让仿真不实际 |
| 什么时候用 | 快速验证 host app 正确性 | 在上板前验证综合后的 RTL |

实践建议：先跑 `swemu` 确认 host app 正确，再跑 `hwemu` 确认 RTL 正确，最后 `hw` 上板。

---

## 术语词典

### Kernel

Kernel 是 Vitis HLS 会综合成 FPGA 硬件的 C/C++ 函数。项目 kernel 放在：

```text
src/kernels/*.cpp                       ← Vitis top：HLS pragma + dataflow 区域
src/kernels/include/kernels/*.hpp       ← 共享头文件：ABI、类型、核心 helper
```

头文件很关键，因为 kernel、cosim testbench、host app 三方都要共享同一套函数签名和 pack 类型。

### Host app

Host app 跑在 CPU 上，用 XRT 做这些事：

1. 打开 FPGA 设备
2. 加载 xclbin
3. 分配输入/输出 buffer object（BO）
4. 把输入数据拷到设备
5. 启动 kernel
6. 把输出拷回
7. 写结果文件

Host app 放在 `src/host/*.cpp`。例如 `run_saxpy`、`run_vadd`、`run_pipeline_demo`。

### Gold reference（真值参考）

Gold reference 是最简单正确的 CPU 实现，是"真值"。它应该容易读，不追求性能。

Gold 代码放在 `src/gold/**`。用它回答："FPGA 正确输出应该是什么？"

### HLS model（HLS 模型）

HLS model 是 CPU 上编译运行的代码，但结构比 gold reference 更接近 HLS kernel。当 kernel 使用 packed vector、stream、dataflow 时，HLS model 特别有用，因为它能在不跑 Vitis 的情况下抓住这些相关的 bug。

HLS model 放在 `src/hls_model/**`。

HLS model **不能证明** timing、资源、Vitis link、XRT 正确性或板卡行为。它只是 CPU 仿真。

### Kernel core（内核核心）

Kernel core 是项目自己的 HLS 兼容代码，同时被 HLS model 和 Vitis kernel top 共用。`saxpy` 的共享 kernel core 在：

```text
src/kernels/include/kernels/saxpy_core.hpp
```

它放 Load/Compute/Store 阶段 helper 和 `SaxpyOp` 函子，不含 host 代码、XRT 代码或板卡配置。

kernel 通过 `ANVIL_DATAFLOW_*` 宏调用这些 helper；HLS model 从普通 CPU C++ 以相同方式调用。

### Kernel ABI 头文件

Kernel ABI 头文件声明 `extern "C"` kernel 签名，是 kernel、cosim testbench 和 host app 三方的契约。例如：

```text
src/kernels/include/kernels/saxpy_kernel.hpp
```

### Kernel pack 类型

两个文件分开定义类型，目的是最大化可移植性：

- **`abi.hpp`** — 只有 pack 宽度常量（如 `kSaxpyPackWidth = 16`），不含 Vitis 或 hlslib 头文件。host 交叉编译安全。
- **`kernel_types.hpp`** — 使用 `anvil::hls::Pack<T, N>` 的 Pack typedef，include 了 HLS 头文件。只供 kernel 和模型代码使用。

这样拆开后，嵌入式 host 交叉编译只需 include `abi.hpp`，不会拉进综合依赖。

### csynth（C 综合）

`csynth` 是 HLS C 综合：Vitis 读取 kernel C++ 代码，生成硬件级报告和编译后的 kernel object（`.xo`）。

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

csynth 回答的问题：

- **Vitis 能接受这段代码吗？** — 部分 C++ 写法无法综合，这是第一个发现处。
- **Initiation Interval (II)** — 流水线接受相邻输入之间的时钟周期数。II=1 是理想值：流水线每时钟处理一个新输入。II=2 意味着吞吐量减半。`#pragma HLS pipeline II=1` 请求 II=1；实际结果取决于数据依赖和内存访问模式。
- **资源估算** — LUT、FF、DSP、BRAM、URAM 利用率。这是 place-and-route 前的估算，实现后会有小幅偏差。
- **估算时钟周期** — 设计能否在目标时钟下满足 timing。

**资源类型对照表：**

| 资源 | 是什么 | kernel 什么时候用更多 |
|---|---|---|
| LUT | 查找表：组合逻辑 | 控制逻辑复杂、运算位宽大 |
| FF | 触发器：流水线寄存器 | 流水线级数多、数据路径宽 |
| DSP | 乘加块 | 浮点或整数乘法运算 |
| BRAM | 块 RAM（18 Kb 单元）：buffer 和 FIFO | 内部缓冲区、FIFO 深度 |
| URAM | Ultra RAM（288 Kb，较新器件）：大 buffer | 大查找表、深队列 |
| HBM | 高带宽存储器（HBM 卡如 U50） | link.cfg 里绑定 HBM 端口时 |

读 csynth 结果：

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

### cosim（C/RTL 协同仿真）

`cosim` 用 C++ testbench 驱动 Vitis 从 kernel 生成的 RTL，验证 RTL 的行为和 C++ kernel 完全一致。

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

Cosim **不是** host app 运行，不加载 xclbin，不使用 XRT。它是 kernel 级 RTL 测试。

cosim 失败就修 kernel 或 testbench，不要先去查 XRT。

### xclbin

Xclbin 是 XRT 加载的 FPGA 二进制文件。Vitis linker 从 kernel object（`.xo`）加 platform 文件和连接配置生成它。

```bash
make xclbin TARGET=u250
```

linker 使用：
1. 综合得到的 kernel object（如 `saxpy.xo`）
2. Platform `.xpfm` 文件（器件特定）
3. `config/<target>/link.cfg` — 连接规则

每个 target/mode 有各自的 xclbin。`sw_emu`、`hw_emu`、`hw` 的 xclbin 不能互换。

### platform / xpfm

Platform 文件（`.xpfm`）告诉 Vitis 目标板卡是什么：器件型号、可用时钟、内存接口、shell 逻辑、支持的构建 flow。

每个 target 配置指向一个 platform 文件。U250 示例：

```make
# config/u250/anvil.mk
ANVIL_PLATFORM := /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
```

这个路径错了，所有 Vitis 步骤都会立即失败。跑综合前务必确认路径存在。

### XRT

XRT（Xilinx Runtime）是 host app 用来和 FPGA 通信的运行时库。它提供打开设备、加载 xclbin、分配 buffer object、启动 kernel、同步数据的 C++ API。

XRT 安装提供：
- 头文件：`/opt/xilinx/xrt/include/`
- 库：`/opt/xilinx/xrt/lib/`
- setup 脚本：`. /opt/xilinx/xrt/setup.sh`

构建 host app 或跑硬件前 source XRT：

```bash
. /opt/xilinx/xrt/setup.sh
xbutil examine   # 应该列出已安装的 FPGA 加速卡
```

### Buffer Object (BO) 与 group_id

Buffer object（BO）是 XRT 管理的一块 host 可访问内存。Host app 分配 BO，填入输入数据，同步到设备，启动 kernel，同步输出回来，读取结果。

BO 必须映射到正确的内存 bank（DDR 或 HBM）。bank 由 kernel 参数位置和 `link.cfg` 连接配置决定：

```cpp
// 参数 index 0 是 kernel 的 `x`，由 link.cfg 映射到 DDR[0]
auto xbuf = xrt::bo(device, size_bytes, kernel.group_id(0));
```

`kernel.group_id(arg_index)` 自动从 xclbin 读取内存 bank 分配。

**BO group index 和 kernel 函数签名里指针参数的顺序一一对应。** 标量参数（如 `alpha`、`n_packs`）不是 BO，没有 group_id。

### emconfig.json

`emconfig.json` 是 XRT 跑 software/hardware emulation 时需要的运行时配置文件，告诉 XRT 要仿真哪个 platform。

生成方式：

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

Anvil 把它存在 `build/<target>/emconfig/emconfig.json`。同一 target 的 `sw_emu` 和 `hw_emu` 共用同一个文件。

跑 emulation 时，`make swemu` 和 `make hwemu` 自动设置 `EMCONFIG_PATH`。手动跑 host app 时需要自己设：

```bash
XCL_EMULATION_MODE=sw_emu EMCONFIG_PATH=build/u250/emconfig ./run_saxpy ...
```

`EMCONFIG_PATH` 错了或文件不存在，XRT 会在打开设备前就失败。

### link.cfg

`link.cfg` 是 Vitis linker 的配置文件，告诉 Vitis 怎么把 kernel compute unit 连到内存 bank。放在 `config/<target>/link.cfg`。

完整格式：

```ini
[connectivity]
# 创建 compute unit：nk=<top function 名>:<数量>:<实例名>
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1

# 把 kernel 指针参数绑定到内存 bank：
# sp=<实例名>.<参数名>:<内存 bank>
# 标量参数（alpha, n_packs 等）不写在这里
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]
sp=vadd_1.a:DDR[0]
sp=vadd_1.b:DDR[1]
sp=vadd_1.out:DDR[2]

[clock]
# 请求时钟频率：freqHz=<频率（Hz）>:<实例名>
freqHz=300000000:saxpy_1
freqHz=300000000:vadd_1
```

关键规则：
- `nk=` 的函数名必须和 C++ `extern "C"` 函数名完全一致。
- `sp=` 的参数名必须和 kernel 函数参数名完全一致。
- **标量参数**（`alpha`、`beta`、`n_packs` 等）是控制寄存器参数，不需要 `sp=` 条目。
- 内存 bank 名（`DDR[0]`、`HBM[0:3]`）是 platform 相关的。不同卡命名不同。
- `nk=` 里的实例名（如 `saxpy_1`）必须和 host app 的 `GetKernel()` 调用一致：

```cpp
ctx.GetKernel("saxpy:{saxpy_1}");
```

### xrt.ini

`xrt.ini` 是控制 XRT profiling 和 debug 输出的运行时配置文件，放在 `config/<target>/xrt.ini`。典型内容：

```ini
[Runtime]
verbosity = 5

[Debug]
timeline_trace = true
data_transfer_trace = coarse
```

把它和 host 可执行文件放在同一目录（或设置 `XILINX_XRT_INI`）。

### Dataset（数据集）

Dataset 是输入和参考输出目录：

```text
data/<name>/
  meta.json       ← 生成这组数据时用的大小、dtype、参数
  x.bin           ← 输入数组（二进制）
  y.bin           ← 输入数组（二进制，如果 kernel 需要两个输入）
  gold_out.bin    ← 期望输出（由 make gold 写入）
```

生成数据集：

```bash
make gen  DATASET=tiny
make gold DATASET=tiny
```

运行输出放在 `runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/`，不写回 `data/`。`data/` 只放输入。

---

## Saxpy 文件地图：一个完整的例子

Demo `saxpy` 按职责刻意拆成这些文件：

```text
src/gold/cpp/saxpy_gold.cpp                         ← CPU 真值实现
src/kernels/include/kernels/abi.hpp                 ← pack 宽度常量，host 编译安全
src/kernels/include/kernels/kernel_types.hpp        ← SaxpyPack typedef
src/kernels/include/kernels/saxpy_kernel.hpp        ← extern "C" ABI 声明
src/kernels/include/kernels/saxpy_core.hpp          ← Load/Compute/Store + SaxpyOp
src/kernels/saxpy_kernel.cpp                        ← Vitis top：HLS pragma + dataflow
src/hls_model/saxpy_hls_model.cpp                  ← CPU 模型，结构镜像 kernel
src/host/run_saxpy.cpp                             ← XRT host app
```

- `saxpy_gold.cpp` 在 CPU 上计算正确结果。简单标量循环。
- `abi.hpp` 定义 `kSaxpyPackWidth`，不引入 HLS 头文件。可以被任何地方 include。
- `kernel_types.hpp` 定义 `SaxpyPack = anvil::hls::Pack<float, kSaxpyPackWidth>`。
- `saxpy_kernel.hpp` 声明 kernel、cosim、host 三方共享的 `extern "C"` 签名。
- `saxpy_core.hpp` 放 Load/Compute/Store 非模板 wrapper 和 `SaxpyOp` 函子。kernel 和 HLS model 共用。
- `saxpy_kernel.cpp` 是 Vitis top：设置 `#pragma HLS INTERFACE`，创建 stream，通过 `ANVIL_DATAFLOW_*` 调用核心 helper。
- `saxpy_hls_model.cpp` 打包 scalar float，调用同样的核心 helper，解包结果。CPU 仿真。
- `run_saxpy.cpp` 打开设备，加载 xclbin，分配 BO，跑 kernel，写结果。

---

## hlslib 框架辅助头文件

Vitis-Anvil 在 `include/anvil/hls/` 下封装了选定的 hlslib 原语，统一命名空间。

| 头文件 | 提供 |
|---|---|
| `pack.hpp` | `anvil::hls::Pack<T,N>`、`PackTraits`、`GetLane`、`SetLane` |
| `stream.hpp` | `anvil::hls::Stream<T,Depth>`、`kDefaultStreamDepth`、`kDefaultDataflowStreamDepth` |
| `dataflow.hpp` | `ANVIL_DATAFLOW_INIT`、`ANVIL_DATAFLOW_FUNCTION`、`ANVIL_DATAFLOW_FINALIZE` 宏 |
| `packed_ops.hpp` | `LoadPacks`、`StorePacks`、`MapPacks`、`MapPacksWithScalar`、`MapMem2Packs` |
| `axis.hpp` | `WriteAxis`、`ReadAxis` — AXI stream 端口的兼容 helper |

这些是框架代码。从你的 kernel include 它们；不要修改。

你的项目 kernel 头文件放在 `src/kernels/include/kernels/`。不要把自己的 kernel 类型加到 `include/anvil/`。

---

## 代码边界：框架 vs 项目

```text
include/anvil/**         ← 框架。不要在这里加你的类型。
src/anvil/**             ← 框架实现。除非修 bug，否则不要改。

src/kernels/**           ← 你的 kernel 代码。随意编辑。
src/kernels/include/**   ← 你的 kernel ABI 头文件和共享核心。
src/hls_model/**         ← 你的 HLS CPU 模型。
src/gold/**              ← 你的 golden reference。
src/host/**              ← 你的 XRT host application。
config/**                ← 你的板卡/target 配置。
```

---

## HLS pragma 速查

这些 pragma 写在 kernel `.cpp` 文件里 Vitis top 函数体内。它们是给 HLS 综合器的指令，不是可执行代码。

### 接口 pragma

```cpp
// AXI master 端口：用于指针参数（DDR/HBM 内存访问）
#pragma HLS INTERFACE m_axi port=x bundle=gmem0 offset=slave depth=1024

// AXI lite 端口：用于标量参数和返回值（控制寄存器）
#pragma HLS INTERFACE s_axilite port=x       bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control
```

规则：
- 每个**指针参数**需要一个 `m_axi` pragma。`bundle=gmem0` 命名 AXI master 接口，通过 `link.cfg` 匹配到内存 bank。
- 每个**标量参数**和 `return` 需要一个 `s_axilite` pragma，`bundle=control`。
- `depth=1024` 是给 cosim 仿真的提示，设为端口访问的最大元素数（不是 pack 数）。

### Pipeline pragma

```cpp
#pragma HLS pipeline II=1
```

写在循环里面。请求 initiation interval 1 — 每个时钟周期启动一次新的循环迭代。没有循环携带的数据依赖或内存端口冲突时 HLS 能实现 II=1。

### Dataflow pragma

```cpp
#pragma HLS dataflow
```

启用跨函数调用的任务级流水线。要求函数之间只通过 stream 或 `#pragma HLS stream` 变量通信。`ANVIL_DATAFLOW_*` 宏封装了这个模式。

---

## 下一步读什么

- **第一次用**：读 [快速开始](get_started.md)，有完整的端到端运行步骤。
- **加自己的算法**：读 [自定义指南](customization.md)。
- **用加速卡（U250、U50、U55C、...）**：读 [加速卡流程](accelerator_flow.md)。
- **用嵌入式板（ZCU102、KV260、...）**：读 [Embedded 流程](embedded_flow.md)。
- **hlslib kernel 骨架**：读 [hlslib 适配](hlslib_adaptation.md)。
- **理解构建缓存模型**：读 [构建系统模型](build_system.md)。
