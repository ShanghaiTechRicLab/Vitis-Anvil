# 项目结构：每一类文件放在哪里

这篇从 FPGA 加速器项目的角度解释仓库结构。核心思想是分层：框架代码、kernel 代码、host 代码、数据代码、板卡配置不要混在一起。

## 顶层目录

| 路径 | 是什么 | 为什么存在 |
|---|---|---|
| `include/anvil/` | 框架公共头文件 | 可复用、可安装/导出的通用辅助 |
| `src/anvil/` | 框架实现 | runtime/logging 等框架库实现 |
| `src/kernels/` | HLS kernel 实现 | Vitis HLS 会把这里的代码转成硬件 |
| `src/kernels/include/kernels/` | 项目 kernel ABI 头文件 | 给 kernel、host、testbench 共享声明 |
| `src/host/` | XRT host app | CPU 程序，加载 xclbin 并启动 kernel |
| `src/gold/` | CPU 真值/reference | 简单正确实现，用于比较 |
| `src/hls_model/` | CPU 编译的 HLS 风格模型 | 综合前检查 pack/stream 算法行为 |
| `src/apps/` | 工具 CLI | 数据生成、gold runner、比较 runner |
| `tests/` | C++/Python/cosim/install 测试 | 在硬件前验证每一层 |
| `config/` | 按板卡拆分的配置 | xpfm 路径、part、xrt.ini、link.cfg、Make 变量 |
| `tools/` | Python 分析工具 | HLS 报告解析、比较、汇总 |
| `scripts/` | Shell/Python 流程辅助 | 板端运行、数据辅助、旧 wrapper |
| `docs/` | 用户文档 | 如何使用和适配模板 |
| `build/` | 生成的构建目录 | CMake 创建，不要编辑或提交 |
| `data/` | 生成的数据集 | 示例输入和参考输出 |
| `runs/` | 运行输出 | 按 target/mode/host/dataset 拆分的 run 产物 |
| `reports/` | 生成的分析报告 | hlsflow 的 HTML/TXT/JSONL 输出 |

## 框架代码 vs 项目代码

框架代码是 Anvil 基础设施，多数用户不应该改：

```text
include/anvil/**
src/anvil/**
```

项目代码是你添加加速器的地方：

```text
src/kernels/**
src/kernels/include/kernels/**
src/hls_model/**
src/gold/**
src/host/**
config/**
tests/**
```

为什么重要：如果你把算法放进 `include/anvil/`，它就变成框架 API，可能被安装给下游用户。`ScaleAddPack` 这种 kernel 专用类型不应该是框架 API。

## Kernel 文件

一个典型 kernel 有三个文件：

```text
src/kernels/include/kernels/my_kernel.hpp   # ABI 和类型声明
src/kernels/my_kernel.cpp                   # HLS 实现
tests/kernels/my_kernel_cosim_tb.cpp        # C/RTL cosim testbench
```

头文件声明 top function：

```cpp
extern "C" void my_kernel(...);
```

`.cpp` 用 HLS pragma 实现它。testbench 用普通 C++ array/vector 调用同一个函数。

## Host app 文件

Host app 在：

```text
src/host/run_saxpy.cpp
src/host/run_vadd.cpp
src/host/run_pipeline_demo.cpp
```

Host app 不会被综合。它在 CPU 上运行并使用 XRT。它需要知道：

- xclbin 路径
- kernel compute-unit 名，例如 `saxpy:{saxpy_1}`
- buffer 参数顺序
- dataset 输入/输出文件名

Host 可执行文件在 `src/host/CMakeLists.txt` 注册。

## 板卡配置文件

每个 target 有一个目录：

```text
config/u250/
config/zcu102/
```

重要文件：

| 文件 | 含义 |
|---|---|
| `anvil.mk` | Make 变量：platform 路径、preset 名、target 类型、kernel target 列表 |
| `link.cfg` | Vitis linker connectivity：compute unit、DDR/HBM bank、clock |
| `pipeline_demo.cfg` | 可选 stream pipeline connectivity |
| `xrt.ini` | XRT runtime tracing/debug 设置 |
| `README.md` | 该板卡/platform 的说明 |

`TARGET=u250` 的意思是“加载 `config/u250/anvil.mk`，使用其中描述的 preset/platform”。

## Build 目录

CMake 把生成文件写到 `build/<preset>/`。例子：

```text
build/hls-model-linux-debug/
build/gold-linux-debug/
build/u250-host/
build/zcu102-kernel/
build/zcu102-host/
```

不要编辑 `build/` 下的文件。如果生成结果不对，应该改源 CMake/config 文件，然后重新 configure。

## 数据和报告

Dataset 目录包含输入和参考输出：

```text
data/tiny/meta.json
data/tiny/x.bin
data/tiny/y.bin
data/tiny/gold_out.bin
```

硬件/仿真运行输出单独保存，例如：

```text
runs/u250/hw_emu/run_saxpy/tiny/latest/out.bin
runs/u250/hw_emu/run_saxpy/tiny/latest/run.json
runs/u250/hw_emu/run_saxpy/tiny/latest/stdout.log
```

Report 目录包含分析产物：

```text
reports/*.html
reports/*.txt
reports/runs.jsonl
reports/runs.csv
```

数据和报告是流程输出，不是源代码。

## 常见任务该改哪里

| 目标 | 先改这些文件 |
|---|---|
| 加新 kernel | `src/kernels/include/kernels/*.hpp`, `src/kernels/*.cpp`, `src/kernels/CMakeLists.txt` |
| 加 cosim | `tests/kernels/*_cosim_tb.cpp`, `src/kernels/CMakeLists.txt` |
| 加 host app | `src/host/*.cpp`, `src/host/CMakeLists.txt` |
| 加 gold reference | `src/gold/include/gold/*.hpp`, `src/gold/cpp/*.cpp`, tests |
| 改 dataset 格式 | `src/apps/gen_dataset.cpp` 或 `scripts/gen_dataset.py`，以及 host/gold/compare 代码 |
| 加板卡 | `config/<target>/`, `CMakePresets.json`, 可选 `platforms/` metadata |
| 改报告 | `tools/hlsflow/**` |
| 改框架 runtime | `include/anvil/runtime/**`, `src/anvil/runtime/**` |

如果你要添加自己的加速器，从 [自定义指南](customization.md) 开始。
