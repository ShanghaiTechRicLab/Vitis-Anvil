# Embedded 流程

这篇解释 ZCU102、ZCU104、ZCU106、KV260 这类 Zynq/ZynqMP embedded 板卡。它们和 PCIe 加速卡不同：host app 跑在板上的 ARM CPU 上，不跑在你的 x86_64 工作站上。

## 1. Embedded 板卡有什么不同？

对 embedded 板卡来说：

- FPGA fabric 和 ARM CPU 在同一块板上
- host app 必须交叉编译成 AArch64
- 板卡镜像里必须有 XRT runtime
- 通常通过 SSH 把 host binary 和 xclbin 拷到板上
- host 编译需要 `PETALINUX_SYSROOT`

流程：

```text
工作站上跑 CPU 测试
  ↓
工作站上 csynth/cosim kernel
  ↓
工作站上链接 embedded xclbin
  ↓
交叉编译 ARM host app
  ↓
复制 host app + xclbin + 数据到板上
  ↓
板上通过 XRT 运行
  ↓
拷回/比较输出
```

## 2. 准备工作站

需要 Vitis 和 embedded platform：

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

还需要匹配板卡镜像的 sysroot：

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

sysroot 里有交叉编译 host app 需要的 ARM 头文件和库。如果 sysroot 和 compiler/toolchain 不匹配，CMake 可能报缺 `crtbeginS.o` 或 `-lgcc`。

## 3. 准备板卡

板上必须装好 XRT。登录板卡后：

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

如果没有 `xbutil` 或看不到设备，先修板卡镜像/XRT 设置。

## 4. 检查 target 配置

打开 `config/zcu102/anvil.mk` 或你的 target。重要字段：

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_PRESET := zcu102-kernel
ANVIL_HOST_PRESET := zcu102-host
ANVIL_PLATFORM ?= /path/to/xilinx_zcu102_base_202420_1.xpfm
```

含义：

- `ANVIL_PRESET` 构建 FPGA kernel/xclbin 侧。
- `ANVIL_HOST_PRESET` 构建 ARM host app 侧。
- `ANVIL_SYSROOT` 指向 ARM sysroot。
- `ANVIL_PLATFORM` 指向 embedded Vitis platform。

## 5. 构建 host app

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

这个命令做什么：

1. 用 AArch64 toolchain 配置 host preset
2. 使用 sysroot 中的目标头文件/库
3. 构建 ARM 可执行文件

输出在：

```text
build/zcu102-host/src/host/run_saxpy
```

如果这一步在编译你的源码前就失败，通常是 sysroot/toolchain 错了。

## 6. 构建 kernel 和 xclbin

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
```

Embedded xclbin link 使用 embedded platform 和它的 memory interface。`link.cfg` 里的 memory 名可能和加速卡不同。

## 7. 生成数据

在工作站上：

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

这会创建输入和期望输出。输入文件必须复制到板上后才能运行硬件。

## 8. 部署到板卡

设置板卡连接变量：

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

复制文件：

```bash
make deploy-bin TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
```

会复制：

- ARM host binary
- xclbin
- `xrt.ini`
- dataset 文件

## 9. QEMU emulation

QEMU 是 embedded simulation 路径，不是加速卡 `hwemu` 的同义词：QEMU 模拟 Arm
PS 侧，通常由 board platform 或 PetaLinux image 提供启动脚本。具体启动命令依赖
target/image，所以 Anvil 使用显式的 `QEMU_LAUNCHER` hook，而不是在 Makefile 里隐藏板卡策略。

通过 board/BSP launcher 运行 QEMU：

```bash
PETALINUX_SYSROOT=/path/to/sysroot \
  make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny \
  QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

该 target 会构建：

- `build/<target>-host/` 下的 AArch64 host binary
- `build/<target>-kernel/` 下的 embedded emulation xclbin
- target 级 `build/<target>/emconfig/emconfig.json`
- `data/<dataset>/` 下的数据集文件

然后执行 `QEMU_LAUNCHER`。launcher 会收到这些环境变量：`HOST_BIN`、`XCLBIN_PATH`、
`DATA_DIR`、`RUN_DIR`、`OUTPUT`、`EMCONFIG_PATH`、`TARGET`、`HOST_APP`、`DATASET`
和 `ANVIL_PLATFORM`。launcher 必须创建 `$OUTPUT`，默认是
`runs/<target>/qemu/<host_app>/<dataset>/<run_key>/out.bin`。

比较 QEMU run 时显式把 `make compare` 指到该输出：

```bash
make compare DATASET=tiny RUN_HW_OUTPUT=runs/zcu102/qemu/run_saxpy/tiny/latest/out.bin
```

## 10. 在板上运行

可以用 all-in-one 目标：

```bash
make test-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

也可以手动登录运行：

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy
. /etc/profile.d/xrt_setup.sh
mkdir -p runs/zcu102/hw/run_saxpy/tiny/latest
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output runs/zcu102/hw/run_saxpy/tiny/latest/out.bin
```

调试时手动运行更好，因为可以直接检查文件和环境。
自动化的 `make test-hw` 路径会由 `scripts/board_run.py` 把远端输出取回到
`runs/<target>/hw/<host_app>/<dataset>/<run_key>/out.bin`，再在工作站侧 compare。

## 11. 比较输出

如果输出已拷回工作站：

```bash
make compare DATASET=tiny
```

如果在板上比较，要保证 compare 工具和 Python 环境也在板上。通常更简单的做法是把输出拷回工作站比较。

## 12. 常见 embedded 失败

### Host build 提示 `SYSROOT environment variable not set`

设置 `PETALINUX_SYSROOT` 或 `ANVIL_SYSROOT`。

### Linker 找不到 `crtbeginS.o` 或 `-lgcc`

sysroot 和 compiler 不匹配。使用匹配 PetaLinux/Vitis release 的 sysroot。

### 板上提示 XRT 缺失

板卡镜像没有 XRT，或者没有 source `/etc/profile.d/xrt_setup.sh`。

### Kernel 能加载但输出错误

按顺序检查：

1. host BO group index
2. kernel 参数顺序
3. `link.cfg` memory binding
4. dataset 文件是否真的复制到板上
5. host app 中的 cache/sync 调用

## 13. 什么时候读 embedded 文档，什么时候读加速卡文档

如果 host app 跑在板上的 ARM CPU 上，读这篇。 如果 host app 跑在插有 PCIe FPGA 卡的 x86_64 主机上，读 [加速卡流程](accelerator_flow.md)。
