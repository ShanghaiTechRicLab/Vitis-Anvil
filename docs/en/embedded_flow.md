# Embedded flow

This page explains Zynq/ZynqMP-style embedded boards such as ZCU102, ZCU104, ZCU106, and KV260. These boards are different from PCIe accelerator cards because the host app runs on the ARM CPU inside the board, not on your x86_64 workstation.

## 1. What is different about embedded boards?

For embedded boards:

- the FPGA fabric and ARM CPU are on the same board
- the host app must be cross-compiled for AArch64
- the board image must contain XRT runtime libraries
- you usually copy the host binary and xclbin to the board over SSH
- `PETALINUX_SYSROOT` is required for host compilation

The flow is:

```text
CPU tests on workstation
  ↓
csynth/cosim kernel on workstation
  ↓
link embedded xclbin on workstation
  ↓
cross-compile host app for ARM
  ↓
copy host app + xclbin + data to board
  ↓
run on board through XRT
  ↓
copy/compare output
```

## 2. Prepare the workstation

You need Vitis and an embedded platform:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

You also need a sysroot matching the board image:

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

The sysroot contains ARM headers and libraries used when cross-compiling the host app. If it does not match the compiler/toolchain, CMake may fail with errors such as missing `crtbeginS.o` or `-lgcc`.

## 3. Prepare the board

On the board, XRT must be installed and usable. After logging in:

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

If `xbutil` is missing or cannot see the device, fix the board image/XRT setup first.

## 4. Check target config

Open `config/zcu102/anvil.mk` or the target you use. Important fields:

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_PRESET := zcu102-kernel
ANVIL_HOST_PRESET := zcu102-host
ANVIL_PLATFORM ?= /path/to/xilinx_zcu102_base_202420_1.xpfm
```

Meaning:

- `ANVIL_PRESET` builds the FPGA kernel/xclbin side.
- `ANVIL_HOST_PRESET` builds the ARM host app side.
- `ANVIL_SYSROOT` points to the ARM sysroot.
- `ANVIL_PLATFORM` points to the embedded Vitis platform.

## 5. Build the host app

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

What this does:

1. configures the host preset with the AArch64 toolchain
2. uses the sysroot for target headers/libs
3. builds an ARM executable

The output is under:

```text
build/zcu102-host/src/host/run_saxpy
```

If this step fails before compiling your source, the sysroot/toolchain is likely wrong.

## 6. Build the kernel and xclbin

```bash
make csynth TARGET=zcu102 KERNEL=saxpy
make cosim TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
```

Embedded xclbin link uses the embedded platform and its memory interfaces. `link.cfg` may use different memory names from accelerator cards.

## 7. Generate data

On the workstation:

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

This creates input and expected output. The input files must be copied to the board before running hardware.

## 8. Deploy to the board

Set board connection variables:

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

Then copy files:

```bash
make deploy-bin TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
```

What gets copied:

- ARM host binary
- xclbin
- `xrt.ini`
- dataset files

## 9. QEMU emulation

QEMU is the embedded simulation path. It is not the same as accelerator-card
`hwemu`: QEMU models the Arm PS side and usually comes from the board platform
or PetaLinux image. The exact launch command is target/image specific, so Anvil
uses an explicit `QEMU_LAUNCHER` hook instead of hiding board policy in the
Makefile.

Run QEMU through a board/BSP launcher:

```bash
PETALINUX_SYSROOT=/path/to/sysroot \
  make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny \
  QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

The target builds:

- AArch64 host binary under `build/<target>-host/`
- embedded emulation xclbin under `build/<target>-kernel/`
- target-level `build/<target>/emconfig/emconfig.json`
- dataset files under `data/<dataset>/`

Then it executes `QEMU_LAUNCHER`. The launcher receives these environment
variables: `HOST_BIN`, `XCLBIN_PATH`, `DATA_DIR`, `RUN_DIR`, `OUTPUT`,
`EMCONFIG_PATH`, `TARGET`, `HOST_APP`, `DATASET`, and `ANVIL_PLATFORM`.
The launcher must create `$OUTPUT`, which defaults to
`runs/<target>/qemu/<host_app>/<dataset>/<run_key>/out.bin`.

Compare a QEMU run by pointing `make compare` at that output:

```bash
make compare DATASET=tiny RUN_HW_OUTPUT=runs/zcu102/qemu/run_saxpy/tiny/latest/out.bin
```

## 10. Run on the board

You can use the all-in-one target:

```bash
make test-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

Or log in and run manually:

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy
. /etc/profile.d/xrt_setup.sh
mkdir -p runs/zcu102/hw/run_saxpy/tiny/latest
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output runs/zcu102/hw/run_saxpy/tiny/latest/out.bin
```

Manual run is better when debugging because you can inspect files and environment directly.
In the automated `make test-hw` path, `scripts/board_run.py` copies the remote
output back to `runs/<target>/hw/<host_app>/<dataset>/<run_key>/out.bin` before
the workstation-side compare step.

## 11. Compare output

If output is copied back to the workstation, run:

```bash
make compare DATASET=tiny
```

If comparing on the board, make sure the compare tool and Python environment exist there. Usually it is simpler to copy output back and compare on the workstation.

## 12. Common embedded failures

### Host build says `SYSROOT environment variable not set`

Set `PETALINUX_SYSROOT` or `ANVIL_SYSROOT`.

### Linker cannot find `crtbeginS.o` or `-lgcc`

The sysroot does not match the compiler. Use a sysroot from the matching PetaLinux/Vitis release.

### Board says XRT missing

The board image does not include XRT or `/etc/profile.d/xrt_setup.sh` was not sourced.

### Kernel loads but output is wrong

Check:

1. host BO group indices
2. kernel argument order
3. `link.cfg` memory bindings
4. dataset files copied to the board
5. cache/sync calls in the host app

## 13. When to use embedded vs accelerator-card docs

Use this page when the host app runs on the board's ARM CPU. Use [Accelerator-card flow](accelerator_flow.md) when the host app runs on the same x86_64 machine that contains a PCIe FPGA card.
