# ZCU102 Board Setup

Xilinx ZCU102 Evaluation Board (Zynq UltraScale+ xczu9eg-ffvb1156-2-e).

## Hardware connections

- JTAG/UART: micro-USB on J2
- Network: GbE on P1 (eth0 on Linux)
- Power: 12 V barrel jack

## Prerequisites on the board

```bash
# Verify XRT is installed and ZCU102 is recognised
xbutil validate -d 0
xbmgmt examine
```

PetaLinux image should include XRT for Zynq UltraScale+. `xrt_setup.sh` is
typically at `/etc/profile.d/xrt_setup.sh`.

## Typical lab IP

Set `BOARD_IP=<your-lab-ip>` when calling `make deploy` or `make test-hw`.
The tracked `[deploy].default_ip` remains empty; use environment variables or
local shell aliases for lab-specific IPs.

## Sysroot for cross-compile

```bash
export PETALINUX_SYSROOT=/opt/petalinux/2024.2/sysroots/cortexa72-cortexa53-xilinx-linux
make build TARGET=zcu102
```
