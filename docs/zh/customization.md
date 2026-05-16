# 自定义指南

Vitis-Anvil 本质是一个模板。保留构建流程和工具链，把 demo 部分换成你自己的 kernel 和 host app。

## 框架代码边界

把 `include/anvil/**` 和 `src/anvil/**` 当作框架基础设施，默认不要改。用户 kernel、ABI 头文件、HLS model、host app 和设备配置放到 `src/kernels/**`、`src/hls_model/**`、`src/host/**` 和 `config/**`。辅助模式见 [hlslib 适配](hlslib_adaptation.md)。

## 1. 添加或替换 kernel

**涉及的文件：** `src/kernels/` 和 `src/kernels/CMakeLists.txt`

在 `src/kernels/` 里写你的 kernel C++ 代码。然后在 `src/kernels/CMakeLists.txt` 中用 `add_anvil_kernel()` 注册：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

如果有 cosim testbench（放在 `tests/kernels/` 里），加上 `TESTBENCH`：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  TESTBENCH     ${CMAKE_SOURCE_DIR}/tests/kernels/my_kernel_cosim_tb.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

运行综合和仿真：

```bash
make csynth TARGET=u250 KERNEL=my_kernel
make cosim TARGET=u250 KERNEL=my_kernel
```

**相关文件：**
- `src/kernels/include/kernels/` — 在这里放你的 kernel ABI 类型头文件
- `tests/kernels/` — 在这里放你的 cosim testbench
- `config/<target>/link.cfg` — 如果 kernel 需要 connectivity 设置

## 2. 添加 host app

**涉及的文件：** `src/host/` 和 `src/host/CMakeLists.txt`

在 `src/host/` 里写你的 host 程序。在 `src/host/CMakeLists.txt` 中用 `add_anvil_host()` 注册：

```cmake
add_anvil_host(run_my_kernel
  LIBS anvil_runtime anvil_gold anvil_log anvil_cli)
```

构建和运行：

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

**相关文件：**
- `src/anvil/runtime/` — host 程序链接的 `XrtContext` 和 buffer 辅助
- `include/anvil/runtime/` — 上述辅助的头文件（`xrt_context.hpp`、`xrt_buffer.hpp`）

## 3. 添加 xclbin connectivity

**涉及的文件：** `config/<target>/link.cfg` 和 `src/kernels/CMakeLists.txt`

编辑或创建 `config/<target>/link.cfg`：

```ini
[connectivity]
nk=my_kernel:1:my_kernel_1
sp=my_kernel_1.input:DDR[0]
sp=my_kernel_1.output:DDR[1]

[clock]
freqHz=300000000:my_kernel_1
```

然后在 `src/kernels/CMakeLists.txt` 注册 xclbin：

```cmake
add_anvil_xclbin(
  NAME my_kernel
  KERNEL_TARGETS my_kernel_xo
  LINK_CFG ${CMAKE_SOURCE_DIR}/config/u250/link.cfg
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

**相关文件：**
- `config/<target>/pipeline_demo.cfg` — 如果你在构建带 kernel-to-kernel 连接的流式 pipeline

## 4. 添加设备

**涉及的文件：** `config/<target>/`、`CMakePresets.json`，可选 `platforms/`

创建 `config/my_board/anvil.mk`。这是告诉构建系统关于你的板卡信息的 Makefile 片段：

```make
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu250-figd2104-2L-e
ANVIL_PLATFORM         := /path/to/platform.xpfm
ANVIL_PRESET           := my-board-host
ANVIL_HWEMU_PRESET     := my-board-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := my_kernel_xo
ANVIL_COSIM_TARGETS    := my_kernel_cosim
```

然后在 `CMakePresets.json` 中添加对应的 configure/build/test presets。

嵌入式设备（ZynqMP 类）在 `config/<target>/anvil.mk` 中用这些设置：

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

**相关文件：**
- `config/<target>/xrt.ini` — 板卡的 XRT 运行时标志
- `config/<target>/README.md` — 你应该写的板卡说明
- `platforms/<board>/` — Python 工具用的 TOML 格式板卡元数据
- `cmake/Toolchain-aarch64-linux.cmake` — 交叉编译 toolchain，有需要可以定制

## 5. 添加平台元数据

**涉及的文件：** `tools/hlsflow/platform_info.py`

`hlsflow` 的设备信息（资源总量、内存说明、默认时钟）在 `tools/hlsflow/platform_info.py` 里。加上你的板卡后，分析报告会显示更有意义的余量百分比。

## 6. 自定义数据集和 golden reference

**涉及的文件：** `scripts/gen_dataset.py`、`src/gold/`、`scripts/compare.py`

数据流程如下：

| 步骤 | 脚本 | 被谁调用 | 写入哪里 |
|---|---|---|---|
| 生成输入 | `scripts/gen_dataset.py` | `make gen` | `data/<dataset>/` |
| Gold reference | `src/gold/cpp/` 或 `src/gold/python/` | `make gold` | `data/<dataset>/` |
| 硬件运行 | host app（如 `src/host/run_saxpy.cpp`） | `make run-host` | `data/<dataset>/` |
| 比较 | `scripts/compare.py` | `make compare` | stdout |

要自定义数据格式：
1. 改 `scripts/gen_dataset.py` 修改生成逻辑。
2. 更新 `src/gold/cpp/` 里的 C++ gold 代码或 `src/gold/python/` 里的 Python gold 代码。
3. 如果比较逻辑需要变化，更新 `scripts/compare.py`。

约定很简单：host app 把输出写到 `data/<dataset>/`，`make compare` 拿这些文件和 golden reference 对比。

## 7. 自定义阈值

**涉及的文件：** `hlsflow check` 命令（通过 Make 或直接运行）

通过 Make 使用 `hlsflow check`：

```bash
make check-hls
```

或者直接运行，自定义任何参数的阈值：

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000 --max-bram 256
```

阈值参数不保存在配置文件中 — 你在命令行传。这样每个检查都是显式的、可审查的。

## 8. 保持命令语义清晰

- `TARGET=` 选择板卡或 platform（对应 `config/<target>/anvil.mk`）
- `KERNEL=` 选择 HLS kernel 目标（对应 `src/kernels/CMakeLists.txt`）和 HLS 报告
- `HOST_APP=` 选择 XRT host 可执行程序（对应 `src/host/CMakeLists.txt`）
- `DATASET=` 选择输入和输出数据集（对应 `data/<dataset>/`）

不要用 `HOST_APP` 来选择 cosim 或 kernel。Cosim 是 kernel 级别的事情，属于 `KERNEL` 的职责范围。
