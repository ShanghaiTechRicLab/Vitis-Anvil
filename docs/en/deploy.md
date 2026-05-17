# Deployment guide

Deployment means moving the files produced on your workstation to the machine that will run the FPGA workload. For PCIe accelerator cards this may be the same machine. For embedded boards it is usually a remote board reached over SSH.

## 1. What files must be present at runtime?

A hardware run needs these files:

| File | Produced by | Used by | Purpose |
|---|---|---|---|
| host binary, e.g. `run_saxpy` | `make build-host` or `make build` | CPU | Program that talks to XRT |
| xclbin, e.g. `saxpy.xclbin` | `make xclbin` | XRT/FPGA | FPGA binary to load |
| `xrt.ini` | `config/<target>/xrt.ini` | XRT | Runtime debug/profile options |
| dataset input files | `make gen` | host app | Input buffers and metadata |
| optional gold output | `make gold` | compare step | Expected output |

If any one is missing, the run may start but fail later.

## 2. Local accelerator-card deployment

For a PCIe card installed in the same machine, “deployment” mostly means building the files in place:

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250
make build TARGET=u250 HOST_APP=run_saxpy
make gen DATASET=tiny
make gold DATASET=tiny
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

There is no SSH copy step because the host app and card are on the same machine.

## 3. Remote embedded deployment

For embedded boards, set connection variables:

```bash
export BOARD_IP=192.168.1.10
export BOARD_SSH_USER=root
export BOARD_DEPLOY_DIR=~/anvil-deploy
```

Then copy the pieces separately:

```bash
make deploy-bin TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=$BOARD_IP
make deploy-xclbin TARGET=zcu102 BOARD_IP=$BOARD_IP
make deploy-data TARGET=zcu102 DATASET=tiny BOARD_IP=$BOARD_IP
make deploy-check TARGET=zcu102 BOARD_IP=$BOARD_IP
```

What each target does:

| Target | What it copies/checks |
|---|---|
| `deploy-bin` | ARM host executable |
| `deploy-xclbin` | FPGA binary and `xrt.ini` |
| `deploy-data` | dataset directory |
| `deploy-check` | verifies remote directory and basic files |
| `deploy` | aggregate copy target |

## 4. Run manually on the board

Manual run is the clearest debugging method:

```bash
ssh root@$BOARD_IP
cd ~/anvil-deploy
. /etc/profile.d/xrt_setup.sh
ls -l
mkdir -p runs/zcu102/hw/run_saxpy/tiny/latest
./run_saxpy --xclbin saxpy.xclbin --data-dir data/tiny --output runs/zcu102/hw/run_saxpy/tiny/latest/out.bin
```

This tells you immediately whether:

- the binary exists and is executable
- xclbin path is correct
- XRT is sourced
- dataset path is correct
- the program fails before or after kernel launch

For the normal `make test-hw` flow, `scripts/board_run.py` retrieves that remote
output into `runs/<target>/hw/<host_app>/<dataset>/<run_key>/out.bin`, and
`make compare` reads the retrieved run output on the workstation.

## 5. All-in-one hardware test

After manual run works, use:

```bash
make test-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=$BOARD_IP
```

This target is convenient, but it hides several steps. If it fails, split it back into deploy and manual run.

## 6. Debug checklist

### SSH fails

Check IP, username, network, and SSH keys/passwords:

```bash
ssh root@$BOARD_IP
```

### Binary says “not found” even though it exists

On embedded Linux this can mean the dynamic loader is missing or incompatible. Check:

```bash
file ./run_saxpy
ldd ./run_saxpy
```

The binary must match the board architecture and sysroot.

### XRT errors before kernel launch

Check:

```bash
. /etc/profile.d/xrt_setup.sh
xbutil examine
```

Also confirm the xclbin was built for the same platform as the board image.

### Kernel name not found

The host app asks for a compute unit such as `saxpy:{saxpy_1}`. That name must exist in `link.cfg` and in the xclbin.

### Output mismatch

Check dataset and argument order:

1. host app reads the correct input files
2. BO group indices match pointer argument order
3. kernel receives element count vs pack count correctly
4. output file path matches compare script

## 7. What to document for a new board

Add `config/<target>/README.md` and record:

- Vitis version used
- platform file path or download/source location
- board image version
- XRT setup command on board
- sysroot path used for host build
- known limitations, such as hw_emu not supported
- exact deploy/run command that worked
