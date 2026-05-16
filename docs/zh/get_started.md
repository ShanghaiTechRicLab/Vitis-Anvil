# 快速开始：跑完整流程

这篇文档从 clone 开始，带你依次跑过 CPU 测试、HLS 综合、协同仿真、xclbin 链接，直到硬件执行。每一步都建立在上一步之上，你可以在任何一步停下来，只要它已经满足你当前的需求。

## 0. 前置条件

仅做 CPU 端工作最少需要：

- CMake 和 Ninja
- C++ 编译器
- Python 3.10 或更高版本
- `make`

做 Vitis 和 XRT 工作还需要：

- Vitis 2024.2（或兼容版本）
- XRT（加速卡的 host 构建和硬件运行需要）
- 匹配的 platform `.xpfm` 文件
- 如果是嵌入式板卡，还需要 PetaLinux sysroot

跑任何 FPGA 流程之前，先 source Vitis 环境：

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

Python 环境需要显式创建 — `make build` 不会自动帮你做：

```bash
make python-env
```

Makefile 会优先使用 `uv`（如果安装了的话），没有 `uv` 就回退到 `python3 -m venv`。默认用 USTC PyPI 镜像，不想用镜像的话：

```bash
make python-env PYPI_INDEX=
```

## 1. CPU-only 快速验证

这是 Day-0 测试。不需要 Vitis、XRT、platform 文件或 FPGA 硬件。

```bash
make test
```

它会编译并运行 C++ 单元测试和 Python 测试。每次 clone 完或做完重构都先跑一遍 — 如果这个通过了，说明你的工具链是好的。

## 2. 选择目标设备

| `TARGET=` | 类型 | 说明 |
|---|---|---|
| `u250` | 加速卡 | 本仓库里测试最充分的路径 |
| `u50` | 加速卡 | 需要安装 U50 的 `.xpfm` |
| `u55c` | 加速卡 | HBM 方向的配置 |
| `u200`, `u280`, `vck5000` | 加速卡 | 有配置脚手架，需要确认本机 platform 包 |
| `zcu102`, `zcu104` | embedded | 测试充分的嵌入式目标 |
| `zcu106`, `kv260` | embedded | 有配置脚手架，需要确认 platform 和 sysroot |

查看本机安装了哪些 platform：

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

如果 `config/<target>/anvil.mk` 里的默认路径和你本机安装位置不同，可以覆盖：

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

## 3. 构建 host 代码

只构建 host app，不触发 kernel 综合：

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

嵌入式目标需要交叉编译，加上 sysroot：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

## 4. 生成数据和 golden output

```bash
make gen DATASET=tiny
make gold DATASET=tiny ANVIL_LANG=cpp
make gold DATASET=tiny ANVIL_LANG=python
```

数据文件放在 `data/<dataset>/` 下。默认 demo 写的是二进制 float buffer 加上元数据。Gold output 就是你之后和硬件结果做对比的基准。

## 5. HLS 综合

用 `KERNEL=` 选择要综合哪个 kernel：

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=vadd
make csynth TARGET=u250 KERNEL=all
```

综合完成后，查看报告：

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

`hlsflow` 会输出带颜色的终端报告、导出 `reports/<run_id>.html` 和 `reports/<run_id>.txt`，并把本次运行追加到 `reports/runs.jsonl`，方便之后对比。

## 6. HLS 协同仿真

Cosimulation 用 kernel 级别的 testbench 来验证 Vitis 从你的 C++ 生成的 RTL。它不跑 XRT host 程序。

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=vadd
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

每个 testbench 跑两次 transaction，这样 Vitis 有足够数据报告 initiation interval 和吞吐量。

## 7. 链接 xclbin

硬件链接很耗时，所以它被设计成一个单独的、显式的步骤。

```bash
make xclbin TARGET=u250
```

硬件仿真（不需要物理板卡）：

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 DATASET=tiny HOST_APP=run_saxpy
```

## 8. 在硬件上运行

加速卡（用真实的 xclbin）：

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

嵌入式板卡（构建、部署、远程运行）：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## 9. 变量说明

| 变量 | 用途 |
|---|---|
| `TARGET=` | 选择板卡或 platform |
| `KERNEL=` | 选择要综合、cosim、分析的 HLS kernel |
| `HOST_APP=` | 选择要构建和运行的 XRT host 程序 |
| `DATASET=` | 选择数据集大小或名称 |
| `ANVIL_PLATFORM=` | 覆盖 `.xpfm` 路径 |
| `PETALINUX_SYSROOT=` | 嵌入式交叉编译的 sysroot |

`KERNEL` 负责 HLS，`HOST_APP` 负责 XRT runtime。它们是两回事，分开管理。
