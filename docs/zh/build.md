# 构建参考

这是简明构建参考。首次使用请先读 [快速开始](get_started.md)。

## 前置条件

| 组件 | 用途 |
|---|---|
| CMake、Ninja、C++ compiler | 所有本地构建 |
| Python 3.10+ | Python 工具和测试 |
| Vitis (`v++`, `vitis-run`) | HLS synthesis、cosim、xclbin |
| XRT | 加速卡 host build 和硬件运行 |
| platform `.xpfm` | Vitis kernel/xclbin 构建 |
| PetaLinux sysroot | embedded AArch64 host build |

## 快速本地构建

```bash
make python-env
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

`make build` 只构建选定 host binary，不创建 Python 环境，也不跑 HLS synthesis。

## HLS 构建

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

## XRT 构建

```bash
make xclbin TARGET=u250
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## Target 覆盖

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50.xpfm
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

变量：`TARGET` 选择平台，`KERNEL` 选择 HLS kernel，`HOST_APP` 选择 XRT host app，`DATASET` 选择数据。
