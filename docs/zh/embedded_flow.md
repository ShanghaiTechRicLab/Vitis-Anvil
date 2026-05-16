# Embedded 流程

本文面向 ZCU102、ZCU104、ZCU106、KV260 等 embedded Xilinx boards。构建拆成 kernel preset 和 AArch64 host preset。

## 1. 需要的组件

- 带 embedded base platform `.xpfm` 的 Vitis。
- PetaLinux 或兼容 AArch64 sysroot。
- 板端 XRT/zocl。
- 可以通过 `scp`/`ssh` 访问板卡。

查找 platform：

```bash
find /tools/Xilinx -name '*zcu102*.xpfm' 2>/dev/null
find /tools/Xilinx -name '*kv260*.xpfm' 2>/dev/null
```

设置 sysroot：

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

路径不同时也可以直接传：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

## 2. 构建 embedded host 和 kernel

Host cross-compile：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

Kernel HLS synthesis：

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make analyze-flow TARGET=zcu102 KERNEL=saxpy
make analyze-cosim TARGET=zcu102 KERNEL=saxpy
```

Link xclbin：

```bash
make xclbin TARGET=zcu102
```

## 3. 部署到板卡

本地生成数据和 gold：

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

部署 artifacts：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

完整板端测试：

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

自定义登录用户和路径：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## 4. 手动板端运行

在板卡上：

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
chmod +x run_saxpy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

拷回输出并比较：

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## 5. Embedded hardware emulation

```bash
make xrt-emu TARGET=zcu102 DATASET=tiny
```

对 embedded targets，这个命令会构建 artifacts 并打印路径。实际 QEMU 启动依赖 BSP/lab 环境，因为 Vitis/PetaLinux platform packaging 差异很大。

## 6. Troubleshooting

| 现象 | 可能原因 | 修复 |
|---|---|---|
| compiler 找不到 `crtbeginS.o` 或 `-lgcc` | sysroot/compiler runtime 不匹配 | 使用和 Vitis/PetaLinux toolchain 匹配的 sysroot |
| 板端 `device not found` | XRT/zocl 未加载 | source XRT setup 并查看 `dmesg` |
| xclbin load error | platform 或 board image 不匹配 | 用匹配的 `.xpfm` 和 boot image 重建 |
| `cosim is not configured` | `KERNEL=` 错误或不支持 stream target | 用 `KERNEL=saxpy`、`KERNEL=vadd`；stream pipeline 当前仅支持加速卡 |
