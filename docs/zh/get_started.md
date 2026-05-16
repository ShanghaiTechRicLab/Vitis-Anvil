# 快速开始：跑完整流程

本文从 clone 后的 CPU 测试开始，一路跑到 HLS synthesis、cosim、xclbin link 和硬件执行。

## 0. 前置条件

CPU-only 工作最少需要：

- CMake 和 Ninja
- C++ compiler
- Python 3.10+
- `make`

Vitis/XRT 工作还需要：

- Vitis 2024.2 或兼容版本
- 加速卡 host build / 硬件运行需要 XRT
- 匹配设备 shell 的 platform `.xpfm`
- Embedded host build 需要 PetaLinux sysroot

FPGA 流程前先 source Vitis：

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

显式创建 Python 环境：

```bash
make python-env
```

Makefile 优先使用 `uv`，没有 `uv` 时 fallback 到 `python3 -m venv`。默认使用 USTC PyPI 源；关闭镜像：

```bash
make python-env PYPI_INDEX=
```

## 1. CPU-only sanity check

这是 Day-0 测试，不使用 Vitis、XRT、platform `.xpfm` 或 FPGA 硬件。

```bash
make test
```

它会跑 C++ unit/smoke tests 和 Python tests。每次 clone 或重构后先跑这个。

## 2. 选择目标设备

常见 target：

| `TARGET=` | 类型 | 说明 |
|---|---|---|
| `u250` | 加速卡 | 当前 repo 的 first-class 路径 |
| `u50` | 加速卡 | 需要你本机实际安装的 U50 `.xpfm` |
| `u55c` | 加速卡 | HBM 方向配置 |
| `u200`, `u280`, `vck5000` | 加速卡 | 配置脚手架；需要确认本机 platform package |
| `zcu102`, `zcu104` | embedded | first-class embedded 路径 |
| `zcu106`, `kv260` | embedded | 配置脚手架；需要确认 platform/sysroot |

查找已安装 platform：

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

如果本机安装路径和默认配置不同，用 `ANVIL_PLATFORM=` 覆盖：

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

## 3. 构建 host 代码

只构建选定 host app，不触发 HLS synthesis：

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

Embedded host cross-compile：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

## 4. 生成数据和 gold output

```bash
make gen DATASET=tiny
make gold DATASET=tiny ANVIL_LANG=cpp
make gold DATASET=tiny ANVIL_LANG=python
```

数据位于 `data/<dataset>/`。默认 demo 写 binary float buffers 和 metadata。

## 5. HLS synthesis

用 `KERNEL=` 选择 kernel-level build target：

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=vadd
make csynth TARGET=u250 KERNEL=all
```

分析报告：

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

`hlsflow` 会输出 rich terminal report、`reports/<run_id>.html`、`reports/<run_id>.txt`，并追加到 `reports/runs.jsonl`。

## 6. HLS cosim

Cosim 跑的是 kernel testbench，不跑 XRT host 程序。

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=vadd
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Testbench 会跑两次 transaction，因此 Vitis 可以报告 interval/throughput。

## 7. Link xclbin

硬件 link 显式触发，通常耗时较长：

```bash
make xclbin TARGET=u250
```

硬件仿真：

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 DATASET=tiny HOST_APP=run_saxpy
```

## 8. 硬件运行

加速卡：

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Embedded board：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## 9. 命令变量规则

| 变量 | 用途 |
|---|---|
| `TARGET=` | 选择 board/platform |
| `KERNEL=` | 选择 HLS synthesis、cosim、HLS report analysis 的 kernel |
| `HOST_APP=` | 选择 XRT host 程序 |
| `DATASET=` | 选择数据集 |
| `ANVIL_PLATFORM=` | 覆盖 `.xpfm` 路径 |
| `PETALINUX_SYSROOT=` | embedded host cross-compile sysroot |

保持这个拆分：`KERNEL` 管 HLS，`HOST_APP` 管 XRT runtime。
