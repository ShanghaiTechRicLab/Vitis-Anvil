# Embedded 流程

嵌入式板卡（ZCU102、ZCU104、ZCU106、KV260）不走 PCIe。构建拆成两个 preset：一个给 kernel，一个给 AArch64 host。

## 1. 需要什么

- 带 embedded base platform `.xpfm` 的 Vitis
- PetaLinux 或兼容的 AArch64 sysroot
- 板端镜像上的 XRT 和 zocl
- 能通过 `scp`/`ssh` 访问板卡

查找 platform 文件：

```bash
find /tools/Xilinx -name '*zcu102*.xpfm' 2>/dev/null
find /tools/Xilinx -name '*kv260*.xpfm' 2>/dev/null
```

设置 sysroot。典型路径：

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

如果你的 sysroot 在其他位置，直接传参：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

## 2. 构建 embedded host 和 kernel

交叉编译 host 程序：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

综合 kernel：

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make analyze-flow TARGET=zcu102 KERNEL=saxpy
make analyze-cosim TARGET=zcu102 KERNEL=saxpy
```

链接 xclbin：

```bash
make xclbin TARGET=zcu102
```

## 3. 部署到板卡

在本地生成数据和 golden output：

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

部署全部内容（二进制、xclbin、数据）：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

完整的端到端测试：

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

可以自定义 SSH 用户和部署目录：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## 4. 手动在板卡上运行

在板卡上：

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
chmod +x run_saxpy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

把输出拷回来并比较：

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## 5. Embedded 硬件仿真

```bash
make xrt-emu TARGET=zcu102 DATASET=tiny
```

对嵌入式目标来说，这个命令会构建 kernel xclbin 和 AArch64 host 二进制，生成 emconfig 文件，然后打印路径。实际的 QEMU 启动取决于具体的 BSP 和 PetaLinux platform 包，那部分不在这个 Makefile 的管理范围内。

## 6. Troubleshooting

| 现象 | 可能原因 | 修复 |
|---|---|---|
| 编译器找不到 `crtbeginS.o` 或 `-lgcc` | Sysroot 和编译器运行时不匹配 | 使用和 Vitis/PetaLinux toolchain 匹配的 sysroot |
| 板端报 `device not found` | XRT 或 zocl 未加载 | Source XRT 环境并查看 `dmesg` |
| xclbin 加载错误 | Platform 或 board image 不匹配 | 用正确的 `.xpfm` 和 boot image 重建 |
| `cosim is not configured` | `KERNEL=` 不对，或该 target 不支持该 kernel 的 cosim | 用 `KERNEL=saxpy` 或 `KERNEL=vadd`；stream pipeline 仅限加速卡 |
