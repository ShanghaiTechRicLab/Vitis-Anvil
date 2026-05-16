# Deployment reference

For the guided embedded flow, see [Embedded flow](embedded_flow.md).

## Build artifacts

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make gen DATASET=tiny
make gold DATASET=tiny
```

## Deploy to board

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Fine-grained deployment:

```bash
make deploy-bin TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-data TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Customize login:

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## Run on board

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Manual board-side run:

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

Copy output back and compare locally:

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## QEMU / hw_emu

`make xrt-emu TARGET=zcu102` builds artifacts and prints paths. Actual QEMU launch is BSP/lab-specific.
