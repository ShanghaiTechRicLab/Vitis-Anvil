# 项目结构

本文说明仓库里每个目录和文件是干什么的。在浏览代码或者把模板适配到自己的项目时，可以用它作为参考。

## 顶层文件

| 文件 | 说明 |
|---|---|
| `Makefile` | 主要的用户接口。所有 `make <target>` 命令都在这里。它封装了 CMake presets，并添加了 HLS、部署、分析等便利目标。 |
| `CMakeLists.txt` | 顶层 CMake 构建定义。定义了 `anvil_core` 接口库，根据构建选项（`ANVIL_BUILD_*`）有条件地包含子目录。 |
| `CMakePresets.json` | CMake 的 configure、build、test presets。每个板卡有自己的 preset，设置了 toolchain 文件、Vitis platform 和 XRT 路径。 |
| `pyproject.toml` | Python 包定义。安装 `anvil` Python 包及其测试依赖。 |
| `.clang-format` | C++ 代码格式化规则。 |

## `cmake/` — CMake 模块

可复用的构建系统组件。每个文件定义一个 CMake 模块或查找器。

| 文件 | 作用 |
|---|---|
| `AnvilKernel.cmake` | 定义 `add_anvil_kernel()` 和 `add_anvil_xclbin()` — 运行 `v++ --compile` 和 `v++ --link` 把 C++ 变成 HLS IP 并链接成 xclbin 的函数。 |
| `AnvilHost.cmake` | 定义 `add_anvil_host()` — 对 `add_executable()` 的薄封装，处理 anvil 库的链接。 |
| `ProjectOptions.cmake` | 声明 `ANVIL_BUILD_*` 选项（GOLD、HLS_MODEL、APPS、KERNELS、XRT、TESTS）。 |
| `BuildOptionValidation.cmake` | 尽早发现 `ANVIL_BUILD_*` 选项的不兼容组合。 |
| `CompilerLauncher.cmake` | 集成 `ccache` 等编译器缓存工具。 |
| `FindVitis.cmake` | 定位 Vitis 安装（`v++`、头文件、platform 包）。 |
| `FindXRT.cmake` | 定位 XRT 安装（头文件、库）。 |
| `Toolchain-aarch64-linux.cmake` | AArch64（嵌入式 ZynqMP 板卡）交叉编译 toolchain 文件。 |
| `Toolchain-x86_64-linux.cmake` | x86_64 Linux host 构建的 toolchain 文件。 |
| `parse_hls_report.py` | 封装 HLS 报告解析的 Python helper，构建系统内部使用。 |
| `anvilConfig.cmake.in` | 安装库后 `find_package(anvil)` 支持使用的模板。 |

## `config/<target>/` — 按板卡配置

每个板卡（u250、zcu102 等）有自己的目录，包含设备相关的文件。

| 文件 | 说明 |
|---|---|
| `anvil.mk` | Makefile 片段，被顶层 `Makefile` 引入。设置板卡特定的变量，如 `ANVIL_PLATFORM`、`ANVIL_VITIS_PART`、`ANVIL_PRESET`、`ANVIL_KERNEL_TARGETS` 等。 |
| `link.cfg` | Vitis 链接器配置。定义 connectivity（`nk=`、`sp=`）和时钟（`freqHz=`）。支持流式的加速卡还有 kernel-to-kernel 连接设置。 |
| `pipeline_demo.cfg` | 流式 pipeline 配置（仅加速卡）。定义 saxpy_stream 和 vadd_stream 如何连接成 pipeline。 |
| `xrt.ini` | 板卡的 XRT 运行时配置。设置 `Runtime.xrt_profile=true` 等标志。 |
| `README.md` | 板卡说明：platform 路径、shell 版本、已知问题。 |

## `src/` — C++ 源代码

### `src/kernels/` — FPGA kernel 源码

| 文件 | 说明 |
|---|---|
| `saxpy_kernel.cpp` | SAXPY HLS kernel（单个计算单元，读 A 和 X，写 Y）。 |
| `vadd_kernel.cpp` | VADD HLS kernel（向量加法，一对输入 buffer）。 |
| `saxpy_stream_kernel.cpp` | SAXPY 的流式版本，用于 kernel-to-kernel pipeline demo。 |
| `vadd_stream_kernel.cpp` | VADD 的流式版本，用于 kernel-to-kernel pipeline demo。 |
| `CMakeLists.txt` | 用 `add_anvil_kernel()` 注册 kernel，用 `add_anvil_xclbin()` 注册 xclbin。在这里添加你自己的 kernel。 |

### `src/host/` — XRT host 程序

| 文件 | 说明 |
|---|---|
| `run_saxpy.cpp` | SAXPY kernel 的 host 程序。创建 XRT context、分配 buffer、运行 kernel、写输出。 |
| `run_vadd.cpp` | VADD kernel 的 host 程序。 |
| `run_pipeline_demo.cpp` | 流式 pipeline demo 的 host 程序。在一条 chain 里跑两个 kernel。 |
| `CMakeLists.txt` | 用 `add_anvil_host()` 注册 host 程序。在这里添加你自己的 host app。 |

### `src/gold/` — Golden reference（CPU 正确性基准）

| 文件 | 说明 |
|---|---|
| `include/gold/saxpy_gold.hpp` | 项目拥有的 SAXPY golden reference API。 |
| `cpp/saxpy_gold.cpp` | SAXPY golden reference 的 C++ 实现（仅 CPU，不需要 XRT）。 |
| `cpp/saxpy_gold_main.cpp` | 命令行封装，读取数据集并写 gold 输出。 |
| `cpp/metrics.cpp` | 可选的指标计算，部分测试用到。 |
| `python/` | Python 版本的 golden reference 实现。 |
| `CMakeLists.txt` | 定义 `anvil_gold` 库和 `saxpy_gold_bin` 可执行文件。 |

### `src/hls_model/` — HLS CPU 模型

| 文件 | 说明 |
|---|---|
| `saxpy_hls_model.cpp` | 镜像 SAXPY kernel 实现的 C++ 模型。用于在综合前验证 kernel 算法是否符合预期。 |

### `src/apps/` — CLI 工具程序

| 文件 | 说明 |
|---|---|
| `gen_dataset.cpp` | 在 `data/<dataset>/` 下生成输入数据集。 |
| `run_gold.cpp` | 运行 golden reference 并写输出供对比。 |
| `compare_gold_hls_model.cpp` | 比较 gold 输出和 HLS 模型输出。 |

### `src/anvil/` — 核心库

| 文件 | 说明 |
|---|---|
| `runtime/xrt_context.cpp` | XRT context 封装 — 设备选择、program 加载、kernel handle 创建。 |
| `runtime/kernel_handle.cpp` | Kernel 参数管理和执行。 |
| `runtime/CMakeLists.txt` | 构建 `anvil_runtime` 静态库（链接 XRT）。 |
| `CMakeLists.txt` | 定义 `anvil_core` 接口库及所有子库（log、cli、json 等）。 |

## `include/anvil/` — C++ 头文件

框架拥有的公共 API。不要把项目专用 kernel/model/gold 头文件放在这里；改用 `src/kernels/include/`、`src/hls_model/include/` 和 `src/gold/include/`。


按组件组织头文件。每个子目录有一个 `*.hpp` 文件。

| 目录 | 提供什么 |
|---|---|
| `runtime/` | `xrt_context.hpp`、`xrt_buffer.hpp`、`kernel_handle.hpp` — XRT 运行时封装。 |
| `gold/` | 通用 gold 辅助，例如 metrics/interfaces。项目专用 gold API 放在 `src/gold/include/`。 |
| `hls/` | 基于 hlslib 的通用辅助：`pack.hpp`、`stream.hpp`、`dataflow.hpp`、`packed_ops.hpp`、`axis.hpp`。 |
| `cli/` | `argparse.hpp` — 命令行参数解析封装。 |
| `compare/` | 验证输出数据的比较器（bitwise、element-wise、classification、signal）。 |
| `json/` | JSON 序列化/反序列化。 |
| `log/` | 日志封装（基于 spdlog）。 |
| `table/` | 终端表格格式化。 |
| `progress/` | 进度条显示（基于 indicators）。 |
| `test/` | 测试工具。 |
| `toml/` | TOML 文件解析（基于 tomlplusplus）。 |
| `config.hpp.in` | 生成的配置头文件模板（版本信息、构建标志）。 |
| `platform.hpp` | 平台检测和能力查询。 |
| `types.hpp` | 通用类型别名。 |

## `tests/` — 测试

| 目录 | 内容 |
|---|---|
| `cpp/` | C++ Catch2 单元测试，覆盖 compare、log、runtime、gold、HLS model。 |
| `kernels/` | HLS cosimulation testbench（`saxpy_cosim_tb.cpp`、`vadd_cosim_tb.cpp` 等）。 |
| `python/` | Python pytest 测试，覆盖 compare、gold、board_run、HLS flow parsers。 |
| `data/` | 测试数据。 |
| `install/` | 安装验证 smoke test。 |
| `CMakeLists.txt` | 注册 CTest 测试（C++ 和 Python）。 |

## `python/anvil/` — Python 包

与 C++ 库组件对应，方便在 Python 中使用。每个模块与 C++ 对应模块同名。

| 文件 | 提供什么 |
|---|---|
| `__init__.py` | 包初始化、版本字符串。 |
| `compare.py` | 输出对比逻辑（C++ compare 的 Python 版本）。 |
| `gold.py` | Python golden reference runner。 |
| `cli.py` | Python CLI 参数辅助。 |
| `json.py` | 测试数据的 JSON 辅助。 |
| `toml.py` | TOML 辅助。 |
| `log.py` | Python 日志配置。 |
| `progress.py` | 进度条显示。 |
| `table.py` | 终端表格格式化。 |
| `test.py` | Python 测试支持工具。 |

## `tools/` — 开发者工具

| 文件 | 作用 |
|---|---|
| `hlsflow/` | HLS 流程分析工具包：发现构建、解析 csynth/cosim/vitis 报告、运行阈值检查、对比 HLS 运行、生成 HTML/TXT/JSONL 报告。详见 `tools/hlsflow/README.md`。 |
| `sweep.py` | 时钟频率扫描工具。接受 TOML 配置文件，在不同时钟目标下运行多次构建。 |

## `scripts/` — 构建和部署脚本

| 文件 | 作用 |
|---|---|
| `gen_dataset.py` | 生成输入数据集。被 `make gen` 调用。 |
| `run_gold.sh` | 运行 golden reference。被 `make gold` 调用。 |
| `compare.py` | 比较硬件输出和 golden reference。被 `make compare` 调用。 |
| `analyze.py` | 旧版 HLS 报告解析器。被 `make analyze-legacy` 调用。 |
| `board_run.py` | 通过 SSH 部署二进制到板卡、运行 host app、取回输出。被 `make test-xrt-hw` 调用。 |
| `emconfig.sh` | 为硬件仿真生成 `emconfig.json`。被 `make emconfig` 调用。 |

## `config/` — 按板卡的 Makefile 配置

| 子目录 | 板卡 |
|---|---|
| `u250/` | Alveo U250 |
| `u50/` | Alveo U50 |
| `u55c/` | Alveo U55C |
| `u200/` | Alveo U200 |
| `u280/` | Alveo U280 |
| `vck5000/` | Versal VCK5000 |
| `zcu102/` | ZynqMP ZCU102 |
| `zcu104/` | ZynqMP ZCU104 |
| `zcu106/` | ZynqMP ZCU106 |
| `kv260/` | Kria KV260 |
| `README.md` | 说明如何添加新板卡。 |

## `data/` — 数据集

| 子目录 | 内容 |
|---|---|
| `tiny/` | 小规模 demo 数据集。`DATASET=tiny` 的默认值。 |
| `empty/` | 空数据集，用于边界测试。 |
| `badshape/` | 维度不匹配的数据集，用于错误路径测试。 |
| `task15_smoke/` 到 `task16_ok/` | 特定测试场景使用的数据集。 |

## `reports/` — HLS 分析输出

由 `make analyze-flow` 和 `make analyze-cosim` 生成。包含：

- `reports/<run_id>.html` / `reports/<run_id>.txt` — 每次 HLS 运行的报告
- `reports/runs.jsonl` — 所有 HLS 运行的 JSONL 数据库

## `third_party/` — 第三方依赖

| 库 | 用途 |
|---|---|
| `argparse/` | 命令行参数解析（header-only）。 |
| `catch2/` | C++ 单元测试框架（Catch2 合并头文件）。 |
| `hlslib/` | HLS 工具库（仿真辅助、dataflow 模式）。 |
| `indicators/` | 终端进度条。 |
| `nlohmann/` | JSON 序列化（`nlohmann/json`）。 |
| `spdlog/` | 日志框架。 |
| `tabulate/` | 终端表格格式化。 |
| `tomlplusplus/` | TOML 配置文件解析。 |

## `platforms/` — 板卡平台元数据

TOML 格式的板卡描述，供 Python 工具查询平台属性。

| 子目录 | 板卡 |
|---|---|
| `zcu102/` | ZCU102 板卡元数据。 |
| `kv260/` | KV260 板卡元数据。 |

## `docs/` — 文档

| 路径 | 内容 |
|---|---|
| `en/` | 英文文档。 |
| `zh/` | 中文文档。 |
| `superpowers/` | 设计文档和规格。 |
| `assets/` | 图片和 banner。 |

## 构建阶段与目录的映射关系

| 阶段 | 构建什么 | 关键 CMake 选项 | 源码目录 |
|---|---|---|---|
| Gold reference | CPU 正确性基准 | `ANVIL_BUILD_GOLD` | `src/gold/` |
| HLS model | Kernel 的 CPU 仿真 | `ANVIL_BUILD_HLS_MODEL` | `src/hls_model/` |
| Utility apps | 数据集生成、gold runner、compare | `ANVIL_BUILD_APPS` | `src/apps/` |
| FPGA kernels | HLS 综合 | `ANVIL_BUILD_KERNELS` | `src/kernels/` |
| XRT host | XRT host 程序 | `ANVIL_BUILD_XRT` | `src/host/` |
| Tests | 单元测试、cosim、Python 测试 | `ANVIL_BUILD_TESTS` | `tests/` |
