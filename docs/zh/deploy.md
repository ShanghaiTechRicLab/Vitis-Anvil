# 部署指南

部署指的是把工作站上生成的文件放到真正运行 FPGA 工作负载的机器上。对 PCIe 加速卡，这通常就是同一台机器。对嵌入式板卡，通常是通过 SSH 连接的远端板子。

## 1. 运行时需要哪些文件？

一次硬件运行需要所有这些文件：

| 文件 | 由谁生成 | 用途 |
|---|---|---|
| Host binary（如 `run_saxpy`） | `make build` 或 `make build-host` | 驱动 XRT 的 CPU 程序 |
| xclbin（如 `saxpy.xclbin`） | `make xclbin` | 要加载的 FPGA 二进制文件 |
| `xrt.ini` | `config/<target>/xrt.ini` | XRT profiling/debug 选项 |
| `emconfig.json` | `make emconfig` | 仅 emulation 模式需要 |
| 数据集输入文件 | `make gen` | 输入 buffer 和元数据 |
| Gold 输出（可选） | `make gold` | 用于对比的期望输出 |

任何文件缺失或来自错误的构建，运行可能开始后在奇怪的地方失败。

---

## 2. 本地加速卡部署

PCIe 卡在同一台机器上，不需要拷贝步骤。完整本地流程：

```bash
# 准备环境
. /tools/Xilinx/Vitis/2024.2/settings64.sh
. /opt/xilinx/xrt/setup.sh
xbutil examine

# 构建
make csynth TARGET=u250 KERNEL=saxpy
make cosim  TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250
make build  TARGET=u250 HOST_APP=run_saxpy
make gen    DATASET=tiny
make gold   DATASET=tiny

# 运行并对比
make hw      TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

同一台机器上的 emulation：

```bash
make emconfig TARGET=u250 MODE=sw_emu
make swemu   TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny

make hwemu   TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

---

## 3. 远端嵌入式板卡部署

### 设置连接变量

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

### 在工作站上构建所有东西

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make csynth TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
make gen    DATASET=tiny
make gold   DATASET=tiny
```

### 拷贝到板卡

```bash
make deploy-bin    TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data   TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
make deploy-check  TARGET=zcu102 BOARD_IP=$BOARD_IP   # 验证文件是否到位
```

每个 target 拷贝什么：

| Target | 拷贝内容 |
|---|---|
| `deploy-bin` | AArch64 host binary |
| `deploy-xclbin` | FPGA 二进制文件和 `xrt.ini` |
| `deploy-data` | 数据集目录（`data/<dataset>/`） |
| `deploy-check` | 验证远端目录有预期的文件 |
| `deploy` | 汇总：依次执行上面这些 target |

---

## 4. 在板卡上手动运行

手动运行是调试板卡问题最直接的方法。SSH 进去之后：

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy

# 每次新 shell 都要 source XRT
. /etc/profile.d/xrt_setup.sh

# 确认 XRT 看到了 FPGA
xbutil examine

# 创建输出目录并运行
mkdir -p runs/zcu102/hw/run_saxpy/tiny/latest
./run_saxpy \
  --xclbin saxpy.xclbin \
  --data-dir data/tiny \
  --output runs/zcu102/hw/run_saxpy/tiny/latest/out.bin
```

手动运行能马上告诉你：
- Binary 是否可执行（架构是否匹配）
- XRT 是否已 source
- Xclbin 路径是否正确
- 数据集路径是否正确
- 程序在 kernel launch 前还是后失败

---

## 5. 一键自动化硬件测试

手动运行通了之后，可以用：

```bash
make test-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

这个 target 会部署、通过 SSH 在板卡上运行、取回输出、与 gold 对比。它隐藏了好几步。如果失败，拆回手动部署和运行。

`scripts/board_run.py` 把远端输出取回到 `runs/<target>/hw/<host_app>/<dataset>/<run_key>/out.bin`，工作站侧的 `make compare` 就可以读到它。

---

## 6. 嵌入式 target 的 QEMU emulation

QEMU 是板卡级仿真路径，和加速卡的 emulation 不是一回事。它模拟 ARM 处理器侧，需要 platform/BSP 特定的 launcher 脚本。

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

Launcher 收到这些环境变量：`HOST_BIN`、`XCLBIN_PATH`、`DATA_DIR`、`RUN_DIR`、`OUTPUT`、`EMCONFIG_PATH`、`TARGET`、`HOST_APP`、`DATASET`、`ANVIL_PLATFORM`。

Launcher 必须创建 `$OUTPUT`，默认路径是 `runs/<target>/qemu/<host_app>/<dataset>/<run_key>/out.bin`。

QEMU 运行完后对比：

```bash
make compare DATASET=tiny RUN_HW_OUTPUT=runs/zcu102/qemu/run_saxpy/tiny/latest/out.bin
```

---

## 7. 调试检查清单

### SSH 连接失败

```bash
ssh root@$BOARD_IP
```

检查：IP 地址、用户名、网络、SSH 密钥或密码。

### "not found" 但文件明明存在

嵌入式 Linux 上这可能是动态加载器缺失或 binary 架构不对：

```bash
file ./run_saxpy
ldd ./run_saxpy
```

Binary 必须和板卡 CPU 架构匹配，且链接的库在板卡镜像里存在。

### Kernel launch 前的 XRT 报错

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

板卡镜像必须包含 XRT 和 setup 脚本。另外检查 xclbin 是否为同一 platform 和 Vitis 版本构建的。

### "Kernel not found" 或 "CU not found"

Host app 要找 `saxpy:{saxpy_1}`，这个实例名必须在 xclbin 里，也就是必须在 `link.cfg` 里以 `nk=saxpy:1:saxpy_1` 存在。两处都要检查。

### 输出不匹配

按这个顺序检查：

1. 正确的输入文件是否部署到了板卡上？
2. Host app 里的 BO group index（index 0 = C++ 签名里第一个指针参数）
3. Kernel 参数顺序（host 传参和 C++ 签名一致吗？）
4. 元素数 vs pack 数（kernel 通常需要 `n_packs` 不是原始元素数）
5. 输出文件路径（compare 脚本读的路径和 host app 写的路径一致吗？）

---

## 8. 每个新板卡都要记录

在 `config/<target>/README.md` 里记录：

- 综合和 link 使用的 Vitis 版本
- Platform 文件路径和获取方式
- 板卡镜像版本和下载地址
- 板卡上的 XRT setup 命令（如 `/etc/profile.d/xrt_setup.sh`）
- 交叉编译用的 sysroot 路径
- 这个 platform 是否支持 hw_emu
- QEMU launcher 细节（如果适用）
- 实际跑通的完整部署和运行命令序列
- 已知问题或限制
