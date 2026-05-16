# Embedded flow

Embedded boards (ZCU102, ZCU104, ZCU106, KV260) do not use PCIe. The build is split into two presets: one for the kernel and one for the AArch64 host.

## 1. What you need

- Vitis with the embedded base platform `.xpfm`
- PetaLinux or a compatible AArch64 sysroot
- XRT and zocl on the board image
- Network access to the board for `scp` and `ssh`

Find platform files:

```bash
find /tools/Xilinx -name '*zcu102*.xpfm' 2>/dev/null
find /tools/Xilinx -name '*kv260*.xpfm' 2>/dev/null
```

Set the sysroot path. This is a typical location:

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

If your sysroot is elsewhere, pass it directly:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

## 2. Build the embedded host and the kernel

Cross-compile the host app:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

Synthesize the kernel:

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make analyze-flow TARGET=zcu102 KERNEL=saxpy
make analyze-cosim TARGET=zcu102 KERNEL=saxpy
```

Link the xclbin:

```bash
make xclbin TARGET=zcu102
```

## 3. Deploy to the board

Generate data and golden output locally:

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

Deploy everything (binary, xclbin, data):

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Full end-to-end test:

```bash
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

You can customize the SSH user and deploy directory:

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/anvil
```

## 4. Manual board run

On the board itself:

```bash
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
chmod +x run_saxpy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

Copy the output back and compare:

```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

## 5. Embedded hardware emulation

```bash
make xrt-emu TARGET=zcu102 DATASET=tiny
```

For embedded targets this builds the kernel xclbin and the AArch64 host binary, generates the emconfig file, and prints the paths. Actual QEMU launch depends on your specific BSP and PetaLinux platform package, so that part remains outside this Makefile.

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Compiler cannot find `crtbeginS.o` or `-lgcc` | Sysroot and compiler runtime do not match | Use a sysroot that matches your Vitis or PetaLinux toolchain |
| `device not found` on the board | XRT or zocl not loaded | Source XRT setup and check `dmesg` |
| xclbin load error | Wrong platform or mismatched board image | Rebuild with the correct `.xpfm` and boot image |
| `cosim is not configured` | Wrong `KERNEL=` value, or the target does not support cosim for that kernel | Use `KERNEL=saxpy` or `KERNEL=vadd`; the stream pipeline is accelerator-card only |
