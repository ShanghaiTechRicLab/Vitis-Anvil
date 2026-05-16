# 用户手册：带上下文的命令速查

读过 [基本概念](concepts.md) 后再用这页。这里是命令速查，但每个命令也说明它做什么。

## 1. 最先检查

```bash
make test
```

运行 CPU-only 测试。Vitis 命令前先跑它。

```bash
make python-env
```

创建 Python 工具用的 `.venv`。普通 `make build` 不会创建它。

## 2. 构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

构建一个 CPU host 可执行文件。它不综合 kernel，也不链接 xclbin。

## 3. HLS kernel 命令

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

对一个 kernel 运行 Vitis HLS 综合。

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

对一个 kernel 运行 C/RTL 协同仿真。

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

汇总 synthesis/cosim 报告。

## 4. xclbin 和硬件

```bash
make xclbin TARGET=u250
```

把综合出的 kernel 链接成 FPGA 二进制。

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

在本机已安装 FPGA 卡的情况下运行 host app。

```bash
make test-xrt-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=192.168.1.10
```

通过 SSH 部署并在远端 embedded 板上运行。

## 5. 数据命令

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make compare DATASET=tiny
```

生成输入、计算 CPU 期望输出、比较硬件输出。

## 6. 变量

| 变量 | 含义 |
|---|---|
| `TARGET` | `config/<target>/` 下的板卡配置 |
| `KERNEL` | HLS kernel target |
| `HOST_APP` | CPU 可执行 target |
| `DATASET` | 数据目录名 |
| `BOARD_IP` | 部署/运行远端板卡 IP |
| `PETALINUX_SYSROOT` | embedded host 构建 sysroot |
| `PYPI_INDEX` | Python 包源；为空表示不用镜像 |

## 7. 更详细的页面

- [构建指南](build.md) 详细解释每个命令。
- [自定义指南](customization.md) 展示如何添加 kernel。
- [部署指南](deploy.md) 解释远端板卡运行。
