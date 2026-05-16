# 部署指南

部署指的是把工作站上生成的文件放到真正运行 FPGA 工作负载的机器上。对 PCIe 加速卡，这可能就是同一台机器。对 embedded 板卡，通常是通过 SSH 连接的远端板子。

## 1. 运行时需要哪些文件？

一次硬件运行需要这些文件：

| 文件 | 谁生成 | 谁使用 | 作用 |
|---|---|---|---|
| host binary，例如 `run_saxpy` | `make build-host` 或 `make build` | CPU | 和 XRT 通信的程序 |
| xclbin，例如 `saxpy.xclbin` | `make xclbin` | XRT/FPGA | 要加载到 FPGA 的二进制 |
| `xrt.ini` | `config/<target>/xrt.ini` | XRT | runtime debug/profile 设置 |
| dataset 输入文件 | `make gen` | host app | 输入 buffer 和 metadata |
| 可选 gold 输出 | `make gold` | compare 步骤 | 期望输出 |

少任何一个，程序都可能启动后在后面失败。

## 2. 本机加速卡部署

如果 PCIe 卡插在同一台机器上，“部署”基本就是在本机把文件构建好：

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250
make build TARGET=u250 HOST_APP=run_saxpy
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

没有 SSH copy 步骤，因为 host app 和卡都在同一台机器上。

## 3. 远端 embedded 部署

设置连接变量：

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

分开复制各类文件：

```bash
make deploy-bin TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
make deploy-check TARGET=zcu102 BOARD_IP=$BOARD_IP
```

每个 target 做什么：

| Target | 复制/检查什么 |
|---|---|
| `deploy-bin` | ARM host 可执行文件 |
| `deploy-xclbin` | FPGA 二进制和 `xrt.ini` |
| `deploy-data` | dataset 目录 |
| `deploy-check` | 检查远端目录和基本文件 |
| `deploy` | 聚合 copy target |

## 4. 在板上手动运行

手动运行最适合调试：

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy
. /etc/profile.d/xrt_setup.sh
ls -l
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

这能立刻告诉你：

- binary 是否存在且可执行
- xclbin 路径是否正确
- XRT 是否已 source
- dataset 路径是否正确
- 程序是在 kernel launch 前失败还是后失败

## 5. All-in-one 硬件测试

手动运行成功后，可以用：

```bash
make test-xrt-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

这个 target 方便，但隐藏了多个步骤。如果失败，把它拆回 deploy 和手动运行。

## 6. 调试清单

### SSH 失败

检查 IP、用户名、网络、SSH key/密码：

```bash
ssh root@$BOARD_IP
```

### Binary 明明存在却提示 “not found”

在 embedded Linux 上，这可能是动态加载器缺失或不兼容。检查：

```bash
file ./run_saxpy
ldd ./run_saxpy
```

binary 必须匹配板卡架构和 sysroot。

### XRT 在 kernel launch 前报错

检查：

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

同时确认 xclbin 是为同一个 board image/platform 构建的。

### 找不到 kernel 名

host app 会请求类似 `saxpy:{saxpy_1}` 的 compute unit。这个名字必须存在于 `link.cfg` 和 xclbin 中。

### 输出不匹配

检查 dataset 和参数顺序：

1. host app 读的是正确输入文件
2. BO group index 和指针参数顺序一致
3. kernel 收到的是 element count 还是 pack count
4. 输出路径和 compare 脚本一致

## 7. 新板卡应该记录什么

添加 `config/<target>/README.md`，记录：

- 使用的 Vitis 版本
- platform 文件路径或下载/来源
- board image 版本
- 板上 XRT setup 命令
- host build 使用的 sysroot 路径
- 已知限制，例如不支持 hw_emu
- 实际跑通的 deploy/run 命令
