# Deployment Guide

This guide covers deploying Vitis-Anvil kernels and host binaries to embedded
boards. Phase 4 provides an end-to-end ZCU102 workflow; KV260 is metadata-only stub support for a later phase.

---

## 1. Development deployment (scp-based)

### Prerequisites

| Step | Command |
|------|---------|
| Build kernel | `make csynth xclbin TARGET=zcu102` |
| Cross-compile host | `make build TARGET=zcu102` (needs `PETALINUX_SYSROOT`) |
| Generate dataset | `make gen DATASET=tiny` |

`PETALINUX_SYSROOT` must point to your PetaLinux 2024.x AArch64 sysroot:
```bash
export PETALINUX_SYSROOT=/opt/petalinux/2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
```

### Deploy and run

```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Fine-grained deploy sub-targets:
```bash
make deploy-bin    TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.100
make deploy-data   TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

Customize SSH user and remote path:
```bash
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 BOARD_SSH_USER=xilinx BOARD_DEPLOY_DIR=/home/xilinx/saxpy
```

### Manual run (after deploy)

```bash
ssh root@192.168.1.100
. /etc/profile.d/xrt_setup.sh
cd ~/anvil-deploy
chmod +x run_saxpy
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output data/tiny/xrt_hw_out.bin
```

Then retrieve output and compare locally:
```bash
scp root@192.168.1.100:~/anvil-deploy/data/tiny/xrt_hw_out.bin data/tiny/
make compare DATASET=tiny
```

---

## 2. Board-side XRT verification

Before running `run_saxpy`, verify the board and XRT are healthy:

```bash
xbutil validate -d 0
xbmgmt examine  # optional; may be unavailable on embedded zocl setups
xbutil validate -d 0 --run DMA
xbutil --version
```

Common failure modes:

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `device not found` | XRT not loaded | `. /etc/profile.d/xrt_setup.sh` |
| `.xclbin` load error | Wrong xclbin for this platform | Rebuild with correct `ANVIL_VITIS_PLATFORM` |
| Mismatched outputs | Data endianness / float format | Verify `gen_dataset.py` matches `run_saxpy` expectations |

---

## 3. PetaLinux / rootfs overlay

For production (not development iteration), burn the AArch64 binary and
xclbin into the PetaLinux rootfs:

```
recipes-apps/saxpy/
├── saxpy.bb
└── files/
    ├── run_saxpy
    └── saxpy.xclbin
```

Minimal `saxpy.bb`:
```bitbake
SUMMARY = "Saxpy XRT demo"
SRC_URI = "file://run_saxpy file://saxpy.xclbin"
S = "${WORKDIR}"
do_install() {
    install -d ${D}/opt/saxpy
    install -m 0755 ${WORKDIR}/run_saxpy ${D}/opt/saxpy/
    install -m 0644 ${WORKDIR}/saxpy.xclbin ${D}/opt/saxpy/
}
FILES:${PN} = "/opt/saxpy/*"
```

Rebuild rootfs with `petalinux-build` and re-flash SD card.

---

## 4. QEMU emulation (hw_emu, ZCU102)

`make xrt-emu TARGET=zcu102` builds the kernel `.xclbin` and the AArch64 host
binary, then prints artifact paths and a QEMU note. Phase 4 does not auto-launch
QEMU and does not generate a repository-owned `run_saxpy_emulation_wrapper`.

```bash
make xrt-emu TARGET=zcu102 DATASET=tiny
```

Expected artifacts after the build:

| Artifact | Path |
|----------|------|
| Kernel xclbin | `build/zcu102-kernel/src/kernels/saxpy_xclbin/saxpy.xclbin` |
| AArch64 host | `build/zcu102-host/src/host/run_saxpy` |
| emconfig | `build/zcu102-kernel/emconfig.json` |

Manual QEMU launch remains lab-specific and depends on the Vitis/PetaLinux
platform packaging. Use the paths printed by `make xrt-emu` together with your
board support package's emulator scripts. Source Vitis first:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
make xrt-emu TARGET=zcu102 DATASET=tiny
```

Record the exact QEMU command used in `reports/` when debugging hw_emu failures.

---

## 5. Failure log collection

When a run fails on the board, collect these files:

```bash
ssh root@192.168.1.100 "tar czf /tmp/anvil_logs.tgz \
    ~/anvil-deploy/*.log \
    /var/log/dmesg* \
    /sys/bus/platform/devices/*/subsystem/drivers/zocl/ 2>/dev/null; \
    echo done"
scp root@192.168.1.100:/tmp/anvil_logs.tgz ./reports/
```

Useful kernel messages:
```bash
dmesg | grep -i xrt
dmesg | grep -i zocl
cat /proc/interrupts | grep xclmgmt
```
