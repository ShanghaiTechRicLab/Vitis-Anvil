# 构建指南：每个命令到底做什么

这篇解释 Make target。它不是简单列命令，而是说明每个 target 消费什么、产生什么。

## 1. 四个选择器

多数命令都接受这些变量：

```bash
make <target> TARGET=u250 KERNEL=saxpy HOST_APP=run_saxpy DATASET=tiny
```

| 变量 | 选择什么 | 例子 | 选错会怎样 |
|---|---|---|---|
| `TARGET` | 板卡/platform 配置 | `u250`, `zcu102` | CMake 用错 platform、part、sysroot 或 link.cfg |
| `KERNEL` | HLS kernel 目标 | `saxpy`, `vadd`, `all` | 综合/cosim 了错误 kernel |
| `HOST_APP` | CPU 可执行程序 | `run_saxpy` | 构建/运行了错误 host 程序 |
| `DATASET` | 输入/输出数据目录 | `tiny` | gold、host、compare 看不同文件 |

## 2. 快速 CPU-only 目标

### `make test`

目的：在使用 Vitis 前先证明 CPU 侧代码正确。

它做什么：

1. 配置 native debug build
2. 构建测试和 CPU 工具
3. 运行 CTest

产物：

- `build/hls-model-linux-debug/` 下的 native binary
- build tree 下的测试数据

什么时候用：

- 改了 C++ 逻辑后
- 改了 Python 工具后
- 跑慢速 HLS 命令前

### `make build TARGET=<target> HOST_APP=<app>`

目的：快速构建一个 host app。

它做什么：

1. 读取 `config/<target>/anvil.mk`
2. 配置 host preset
3. 构建 `src/host/<HOST_APP>`

它不会综合 kernel，也不会构建 xclbin。

例子：

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

## 3. Python 环境目标

部分分析工具使用 Python。普通 `make build` 不会创建虚拟环境。

```bash
make python-env
```

它做什么：

- 如果有 `uv` 就使用 `uv`
- 否则用 `python3 -m venv` 创建 `.venv`
- 安装项目测试依赖
- 默认使用 USTC PyPI 镜像

禁用镜像：

```bash
make python-env PYPI_INDEX=
```

从头重建：

```bash
make rebuild-python
```

## 4. HLS 综合目标

### `make csynth TARGET=<target> KERNEL=<kernel>`

目的：把 kernel C++ 转成 Vitis HLS 输出和报告。

例子：

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

它消费：

- `src/kernels/` 里的 kernel 源码
- `src/kernels/include/kernels/` 里的 ABI 头文件
- `config/<target>/anvil.mk` 里的 target 配置
- 该配置中的 Vitis platform 路径

它产生：

- HLS 工作目录
- `.xo` kernel object 或 packaged HLS 输出
- csynth XML/text 报告

如果失败，先看 Vitis HLS log。常见原因：

- kernel 代码不是 HLS 兼容 C++
- include path 缺失
- platform 路径无效
- 使用了 HLS 不支持的 C++ 特性

### `make analyze-flow TARGET=<target> KERNEL=<kernel>`

目的：读取 HLS 报告，并用人能读的格式显示摘要。

例子：

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
```

通常在 `csynth` 后使用。如果报告已经存在，它本身不重新综合。

## 5. HLS cosim 目标

### `make cosim TARGET=<target> KERNEL=<kernel>`

目的：对一个或多个 kernel 运行 C/RTL 协同仿真。

例子：

```bash
make cosim TARGET=u250 KERNEL=vadd
```

它消费：

- kernel 源码
- `tests/kernels/` 里的 cosim testbench
- Vitis HLS 生成的 RTL

它产生：

- HLS 工作目录里的 cosim 报告
- pass/fail 状态
- 如果可用，会有 latency 信息

Cosim 是 kernel 测试。它不运行 host app，也不加载 xclbin。

### `make analyze-cosim TARGET=<target> KERNEL=<kernel>`

目的：汇总 cosim 报告。

例子：

```bash
make analyze-cosim TARGET=zcu102 KERNEL=all
```

## 6. xclbin 目标

### `make xclbin TARGET=<target>`

目的：把综合出来的 kernel object 链接成 FPGA 二进制。

它消费：

- `saxpy_xo` 这类 kernel object
- `config/<target>/link.cfg`
- Vitis platform `.xpfm`

它产生：

```text
build/<preset>/src/kernels/<name>_xclbin/<name>.xclbin
```

这一步可能很慢。先跑 `csynth` 和 `cosim`，不要等 link 跑很久后才发现 kernel bug。

### `make xclbin-hwemu TARGET=<target>`

目的：构建 hardware emulation 用的 xclbin，而不是真实硬件 xclbin。

当你想测试 host/XRT 流程但没有物理卡时使用，前提是 platform 支持 emulation。

## 7. 数据和正确性目标

### `make gen DATASET=<name>`

目的：生成输入文件。

### `make gold DATASET=<name>`

目的：运行 CPU reference 并写入期望输出。

### `make compare DATASET=<name>`

目的：比较硬件输出和 gold 输出。

典型正确性流程：

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

## 8. Host 运行和部署目标

### `make run-host TARGET=<target> HOST_APP=<app> DATASET=<data>`

目的：在有 FPGA 卡和 XRT 的本机运行 host app。

它要求：

- host binary 已构建
- xclbin 已构建
- dataset 存在
- XRT 能看到设备

### `make deploy-*`

目的：把 host binary、xclbin、数据、配置复制到远端 embedded 板卡。

使用前先读 [部署指南](deploy.md)。

## 9. 我该跑哪个命令？

| 情况 | 命令 |
|---|---|
| 改了普通 C++ 工具代码 | `make test` |
| 改了 host app | `make build TARGET=<target> HOST_APP=<app>` |
| 改了 kernel HLS 代码 | `make csynth TARGET=<target> KERNEL=<kernel>` |
| 改了 kernel 行为 | `make cosim TARGET=<target> KERNEL=<kernel>` |
| 改了 link.cfg | `make xclbin TARGET=<target>` |
| 改了数据格式 | `make gen`, `make gold`, host run, `make compare` |
| 想看报告摘要 | `make analyze-flow`, `make analyze-cosim` |
| 需要 Python 工具 | `make python-env` |

## 10. 常见构建失败

### Platform `.xpfm` 缺失

检查 `config/<target>/anvil.mk`。`ANVIL_PLATFORM` 必须指向已安装的 Vitis platform。

### XRT 缺失

Host build 需要 XRT 头文件和库。source XRT setup，或者如果 XRT 不在 `/opt/xilinx/xrt`，设置 `XILINX_XRT`。

### Embedded host 编译器找不到 libc/gcc 文件

`PETALINUX_SYSROOT` 和编译器/toolchain 不匹配。使用板卡镜像或匹配 PetaLinux release 的 sysroot。

### cosim target 缺失

kernel 注册时没有 `TESTBENCH`，或者 `config/<target>/anvil.mk` 里的 `ANVIL_COSIM_TARGETS` 没有包含它。
