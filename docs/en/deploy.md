# Deployment guide

Deployment means moving build artifacts to the machine that will run the FPGA workload. For PCIe accelerator cards this is usually the same machine. For embedded boards it is a remote board reached over SSH.

## 1. What files are needed at runtime?

A hardware run needs all of these:

| File | Produced by | Purpose |
|---|---|---|
| Host binary (`run_saxpy`) | `make build` or `make build-host` | CPU program that drives XRT |
| xclbin (`saxpy.xclbin`) | `make xclbin` | FPGA binary to load |
| `xrt.ini` | `config/<target>/xrt.ini` | XRT profiling/debug options |
| `emconfig.json` | `make emconfig` | Required for emulation modes only |
| Dataset input files | `make gen` | Input buffers and metadata |
| Gold output (optional) | `make gold` | Expected output for comparison |

If any file is missing or from the wrong build, the run may start and then fail late with a confusing error.

---

## 2. Local accelerator-card deployment

For a PCIe card in the same machine, there is no copy step. The full local sequence is:

```bash
# Environment
. /tools/Xilinx/Vitis/2024.2/settings64.sh
. /opt/xilinx/xrt/setup.sh
xbutil examine

# Build
make csynth TARGET=u250 KERNEL=saxpy
make cosim  TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250
make build  TARGET=u250 HOST_APP=run_saxpy
make gen    DATASET=tiny
make gold   DATASET=tiny

# Run and compare
make hw     TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

For emulation on the same machine:

```bash
make emconfig TARGET=u250 MODE=sw_emu
make swemu  TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny

make hwemu  TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

---

## 3. Remote embedded board deployment

### Set connection variables

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

### Build everything on the workstation

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make csynth TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
make gen    DATASET=tiny
make gold   DATASET=tiny
```

### Copy to the board

```bash
make deploy-bin    TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data   TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
make deploy-check  TARGET=zcu102 BOARD_IP=$BOARD_IP   # verify files arrived
```

What each target copies:

| Target | What it copies |
|---|---|
| `deploy-bin` | AArch64 host binary |
| `deploy-xclbin` | FPGA binary and `xrt.ini` |
| `deploy-data` | Dataset directory (`data/<dataset>/`) |
| `deploy-check` | Verifies remote directory has expected files |
| `deploy` | Aggregate: runs the targets above |

---

## 4. Run manually on the board

Manual run is the clearest way to debug board issues. After SSH-ing in:

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy

# Source XRT (required on every new shell session)
. /etc/profile.d/xrt_setup.sh

# Verify XRT sees the FPGA fabric
xbutil examine

# Create output directory and run
mkdir -p runs/zcu102/hw/run_saxpy/tiny/latest
./run_saxpy \
  --xclbin saxpy.xclbin \
  --data-dir data/tiny \
  --output runs/zcu102/hw/run_saxpy/tiny/latest/out.bin
```

Manual run tells you immediately whether:
- The binary is executable (architecture matches)
- XRT is sourced
- xclbin path is correct
- Dataset path is correct
- The program fails before or after kernel launch

---

## 5. Automated all-in-one hardware test

After manual run works, use:

```bash
make test-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

This deploys, runs on the board through SSH, retrieves the output, and compares against gold. It hides several steps. If it fails, split back into manual deploy and run.

The `scripts/board_run.py` script retrieves remote output to `runs/<target>/hw/<host_app>/<dataset>/<run_key>/out.bin` so the workstation-side `make compare` can read it.

---

## 6. QEMU emulation for embedded targets

QEMU is a board-level simulation path, separate from accelerator-card emulation. It models the ARM processor side and requires a platform/BSP-specific launcher.

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

The launcher receives environment variables: `HOST_BIN`, `XCLBIN_PATH`, `DATA_DIR`, `RUN_DIR`, `OUTPUT`, `EMCONFIG_PATH`, `TARGET`, `HOST_APP`, `DATASET`, `ANVIL_PLATFORM`.

The launcher must create `$OUTPUT`, which defaults to `runs/<target>/qemu/<host_app>/<dataset>/<run_key>/out.bin`.

Compare after QEMU run:

```bash
make compare DATASET=tiny RUN_HW_OUTPUT=runs/zcu102/qemu/run_saxpy/tiny/latest/out.bin
```

---

## 7. Debug checklist

### SSH fails

```bash
ssh root@$BOARD_IP
```

Check: IP address, username, network, SSH keys or passwords.

### "not found" even though the binary exists

On embedded Linux this can mean the dynamic loader is missing or the binary was compiled for the wrong architecture:

```bash
file ./run_saxpy
ldd ./run_saxpy
```

The binary must match the board CPU architecture and be linked against libraries present in the board image.

### XRT errors before kernel launch

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

The board image must include XRT and the setup script. Also check that the xclbin was built for the same platform and Vitis version as the board image.

### "Kernel not found" or "CU not found"

The host app asks for `saxpy:{saxpy_1}`. This instance name must exist in the xclbin, which means it must be in `link.cfg` as `nk=saxpy:1:saxpy_1`. Check both.

### Output mismatch

Check in this order:

1. Are the correct input files deployed to the board?
2. BO group indices in the host app (index 0 = first pointer argument in C++ signature)
3. Kernel argument order (host passes them in the same order as the C++ signature)
4. Element count vs pack count (kernel often expects `n_packs`, not raw element count)
5. Output file path (the compare script must read from the same path the host app wrote to)

---

## 8. Document every new board

Add `config/<target>/README.md` and record:

- Vitis version used for synthesis and link
- Platform file path and how to obtain it
- Board image version and where to download it
- XRT setup command on the board (`/etc/profile.d/xrt_setup.sh` or similar)
- Sysroot path used for cross-compilation
- Whether hw_emu is supported for this platform
- QEMU launcher details if applicable
- Exact deploy and run command sequence that worked
- Known issues or limitations
