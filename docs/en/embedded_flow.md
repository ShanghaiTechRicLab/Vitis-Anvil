# Embedded flow

This flow targets embedded Xilinx boards such as ZCU102, ZCU104, ZCU106, and KV260. The build is split into a kernel preset and an AArch64 host preset.

## 1. Required pieces

- Vitis with embedded base platform `.xpfm`.
- PetaLinux or compatible AArch64 sysroot.
- XRT/zocl on the board image.
- Network access to the board for `scp`/`ssh` deployment.

Find platform files:

```bash
find /tools/Xilinx -name '*zcu102*.xpfm' 2>/dev/null
find /tools/Xilinx -name '*kv260*.xpfm' 2>/dev/null
```

Set the sysroot:

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

If your path differs, pass it on the command line:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

## 2. Build embedded host and kernel

Host cross-compile:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

Kernel HLS synthesis:

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make analyze-flow TARGET=zcu102 KERNEL=saxpy
make analyze-cosim TARGET=zcu102 KERNEL=saxpy
```

Link xclbin:

```bash
make xclbin TARGET=zcu102
```

## 3. Deploy to board

Generate data and gold locally:

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

Deploy artifacts:

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Run complete board test:

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Customize remote login:

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## 4. Manual board run

On the board:

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
chmod +x run_saxpy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

Copy output back and compare:

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## 5. Embedded hardware emulation

```bash
make xrt-emu TARGET=zcu102 DATASET=tiny
```

For embedded targets this builds artifacts and prints paths. Actual QEMU launch remains BSP/lab-specific because Vitis/PetaLinux platform packages differ.

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| compiler cannot find `crtbeginS.o` or `-lgcc` | sysroot/compiler runtime mismatch | use the sysroot that matches the Vitis/PetaLinux toolchain |
| `device not found` on board | XRT/zocl not loaded | source XRT setup and inspect `dmesg` |
| xclbin load error | wrong platform or board image | rebuild with matching `.xpfm` and boot image |
| `cosim is not configured` | wrong `KERNEL=` or unsupported stream target | use `KERNEL=saxpy`, `KERNEL=vadd`; stream pipeline is accelerator-card only |
