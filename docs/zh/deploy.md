# 部署参考

完整 embedded 引导流程见 [Embedded 流程](embedded_flow.md)。

## 构建 artifacts

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make gen DATASET=tiny
make gold DATASET=tiny
```

## 部署到板卡

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

细粒度部署：

```bash
make deploy-bin TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-data TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

自定义登录：

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## 板端运行

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

手动板端运行：

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

拷回输出并本地比较：

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## QEMU / hw_emu

`make xrt-emu TARGET=zcu102` 会构建 artifacts 并打印路径。实际 QEMU 启动依赖 BSP/lab 环境。
