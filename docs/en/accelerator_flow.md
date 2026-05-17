# Accelerator-card flow

This page explains the flow for PCIe accelerator cards such as U250, U50, U55C, U200, U280, and VCK5000. These cards usually run the host app on the same x86_64 machine that contains the FPGA card.

## 1. What is different about accelerator cards?

For an accelerator card:

- the CPU host app runs on your workstation/server
- XRT is installed on that same machine
- the FPGA card is visible through PCIe
- the xclbin is loaded directly by the host app
- no PetaLinux sysroot is needed for normal host builds

The basic path is:

```text
CPU tests
  ↓
csynth/cosim kernel
  ↓
link xclbin for the card platform
  ↓
build host app
  ↓
run host app locally through XRT
  ↓
compare output
```

## 2. Prepare the environment

You need:

1. Vitis installed, for example `/tools/Xilinx/Vitis/2024.2`.
2. XRT installed, usually `/opt/xilinx/xrt`.
3. A platform `.xpfm` for your card.
4. A card visible to XRT if you want to run hardware.

Typical shell setup:

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
. /opt/xilinx/xrt/setup.sh
xbutil examine
```

`xbutil examine` should list the card. If it does not, fix XRT/card installation before debugging Anvil.

## 3. Check the target config

Open `config/u250/anvil.mk` or the target you use. Important fields:

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_PLATFORM       ?= /path/to/platform.xpfm
ANVIL_PRESET         := u250-host
ANVIL_HWEMU_PRESET   := u250-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim
```

Meaning:

- `ANVIL_PLATFORM` is the Vitis platform file.
- `ANVIL_PRESET` selects the CMake preset used for hardware builds.
- `ANVIL_HWEMU_PRESET` selects the preset used for hardware emulation.
- `ANVIL_KERNEL_TARGETS` is what `KERNEL=all` means for synthesis.
- `ANVIL_COSIM_TARGETS` is what `KERNEL=all` means for cosim.

If the platform path is wrong, every Vitis step will fail early.

## 4. Run synthesis and cosim first

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Do this before xclbin link. It is faster to catch kernel problems here.

## 5. Understand `link.cfg`

For accelerator cards, `config/<target>/link.cfg` tells Vitis how kernels connect to memory.

Example shape:

```ini
[connectivity]
nk=saxpy:1:saxpy_1
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]

[clock]
freqHz=300000000:saxpy_1
```

Meaning:

- `nk=saxpy:1:saxpy_1` creates one compute unit named `saxpy_1` from top function `saxpy`.
- `sp=saxpy_1.x:DDR[0]` binds pointer argument `x` to DDR bank 0.
- `freqHz=...` requests a clock for that compute unit.

The host app later opens the kernel by compute-unit name:

```cpp
ctx.GetKernel("saxpy:{saxpy_1}");
```

If `link.cfg` says `saxpy_1` but the host asks for `saxpy_2`, the run fails.

## 6. Link xclbin

```bash
make xclbin TARGET=u250
```

This can take a long time. It produces a file like:

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

After link succeeds, inspect the linked artifact:

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

This prints the xclbin path, compute-unit names, memory connectivity, clocks, and Vitis link warnings/errors.

If link fails, inspect the Vitis link log. Common issues:

- invalid `sp=` argument name
- unsupported memory bank name for the platform
- too many kernels for the device/resources
- platform version mismatch with Vitis

## 7. Build and run the host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

What happens in `run-host`:

1. host app opens device 0
2. loads the xclbin
3. finds compute unit such as `saxpy_1`
4. allocates XRT buffers
5. copies input to the card
6. launches kernel
7. copies output back
8. writes output under `data/tiny/`

If `run-host` fails, determine whether it failed before or after kernel launch:

- before launch: XRT/device/xclbin/kernel-name problem
- after launch: buffer group, data layout, kernel correctness, or compare problem

## 8. Hardware emulation

Hardware emulation runs an emulated device. It is slower than CPU tests but does not require a physical card.

Typical flow:

```bash
make xclbin-hwemu TARGET=u250
make build TARGET=u250 HOST_APP=run_saxpy
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Use hw_emu when debugging host/XRT integration. Do not use it as a substitute for csynth/cosim; those answer different questions.

## 9. Stream pipeline demo

Some accelerator-card targets have `config/<target>/pipeline_demo.cfg`. That enables a kernel-to-kernel stream demo:

```text
saxpy_stream → vadd_stream
```

Build it explicitly:

```bash
make csynth TARGET=u250 KERNEL=pipeline_demo
make cosim TARGET=u250 KERNEL=pipeline_demo
make pipeline-demo TARGET=u250
make build TARGET=u250 HOST_APP=run_pipeline_demo
```

This is opt-in. It is not built by normal `make build` because stream pipeline synthesis/link can be slow.

## 10. Add another accelerator card target

To add a similar PCIe card:

1. Copy a close config directory, for example `config/u250` to `config/my_card`.
2. Edit `config/my_card/anvil.mk`.
3. Set the correct `.xpfm` path.
4. Update `link.cfg` memory bank names to match the platform.
5. Add or copy CMake presets.
6. Add platform metadata in `tools/hlsflow/platform_info.py` so reports show resource percentages.
7. Run `make csynth TARGET=my_card KERNEL=saxpy` before trying xclbin.

Do not assume DDR/HBM bank names are portable across cards. Always check the platform documentation or an existing Vitis example for that card.
