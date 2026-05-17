# Accelerator-card flow

This page covers PCIe accelerator cards: U250, U50, U55C, U200, U280, VCK5000, and similar Alveo-family cards. These cards plug into a PCIe slot. The host app runs on the same x86_64 machine that holds the card.

---

## 1. How accelerator cards differ from embedded boards

| Aspect | Accelerator card | Embedded board |
|---|---|---|
| Host app runs on | Same x86_64 machine | Board ARM CPU (AArch64) |
| XRT location | Host machine | Board image |
| Host compilation | Native x86_64 | Cross-compile with sysroot |
| Data transfer | PCIe DMA | On-chip interconnect |
| Deploy step | Not needed | SSH copy required |

If your board is ZCU102, ZCU104, or KV260, read [Embedded flow](embedded_flow.md) instead.

---

## 2. Prepare the environment

Install and source these tools before any Vitis command:

```bash
# Source Vitis (adjust version and path to match your installation)
. /tools/Xilinx/Vitis/2024.2/settings64.sh

# Source XRT
. /opt/xilinx/xrt/setup.sh

# Verify the card is visible
xbutil examine
```

`xbutil examine` should list your installed Alveo card with its BDF address (e.g. `[0000:65:00.1]`). If it does not appear, fix the XRT/driver installation before debugging Anvil.

You also need a platform `.xpfm` file for your card. Common locations:

| Card | Typical platform path |
|---|---|
| U250 | `/opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/` |
| U50 | `/opt/xilinx/platforms/xilinx_u50_gen3x4_xdma_2_202020_1/` |
| U55C | `/opt/xilinx/platforms/xilinx_u55c_gen3x16_xdma_3_202210_1/` |
| U280 | `/opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/` |

---

## 3. Check the target configuration

Open `config/u250/anvil.mk` (or your target's config):

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_VITIS_PART     := xcu250-figd2104-2L-e
ANVIL_PLATFORM       ?= /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
ANVIL_PRESET         := u250-host
ANVIL_HWEMU_PRESET   := u250-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_XRT_LIB        := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim
```

**The most important line is `ANVIL_PLATFORM`.** If this path does not exist on disk, every Vitis command will fail immediately. Override it on the command line if needed:

```bash
make csynth TARGET=u250 KERNEL=saxpy ANVIL_PLATFORM=/my/path/platform.xpfm
```

---

## 4. Synthesis and cosim first — always

Never go directly to xclbin link. Synthesis and cosim are cheap checks that catch kernel bugs before the slow linking step.

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze TARGET=u250 KERNEL=saxpy    # read the synthesis report
make cosim  TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Check `analyze` output for:
- **II=1**: the pipeline accepts one input per clock cycle. II>1 limits throughput.
- **Timing**: the design must fit within the clock period. Negative slack means timing failure.
- **Resources**: LUT/FF/DSP/BRAM/URAM usage. High DSP count usually means unrolled multiply operations.

---

## 5. Understanding link.cfg

`config/<target>/link.cfg` tells Vitis how to connect kernel compute units to memory banks on the card.

```ini
[connectivity]
# Create compute units from kernel top functions
# Format: nk=<top_function_name>:<count>:<instance_name>
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1

# Bind kernel pointer arguments to memory banks
# Format: sp=<instance_name>.<argument_name>:<memory_bank>
# Only pointer arguments need sp= entries. Scalars do not.
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]
sp=vadd_1.a:DDR[0]
sp=vadd_1.b:DDR[1]
sp=vadd_1.out:DDR[2]

[clock]
# Request clock frequency for compute units
# Format: freqHz=<Hz>:<instance_name>
freqHz=300000000:saxpy_1
freqHz=300000000:vadd_1
```

**Key rules:**
- `nk=` names must match the C++ `extern "C"` function name exactly.
- `sp=` argument names must match the kernel function parameter names exactly. If the kernel says `void saxpy(const float* x, ...)`, then `sp=` says `saxpy_1.x:DDR[0]` — not `in`, not `a`.
- **Scalar arguments** (`alpha`, `n_packs`, etc.) are control register arguments. Do NOT add them to `sp=`.
- Memory bank names (`DDR[0]`, `HBM[0:3]`) are platform-specific. U250 has DDR banks; U50 has HBM banks. Check the platform documentation.
- The instance name from `nk=` (e.g. `saxpy_1`) must match what the host app asks for:

```cpp
ctx.GetKernel("saxpy:{saxpy_1}");  // host app uses the nk= instance name
```

**Common link.cfg errors:**
- `sp=saxpy_1.in` when the kernel argument is named `x` — names must match exactly
- `sp=saxpy_1.alpha:DDR[0]` — scalars do not get memory bindings
- Using `HBM[0]` on a DDR-only card like U250

---

## 6. Link the xclbin

```bash
make xclbin TARGET=u250
```

This produces:

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

After linking, always inspect what ended up in the xclbin before touching the host app:

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

This shows the compute unit names, memory port bindings, clock frequencies, and any link warnings. **Fix problems here, not in the host app.** If the xclbin does not have `saxpy_1`, the host app cannot fix it.

Hardware xclbin builds are slow. Software and hardware emulation builds are faster but still take minutes.

---

## 7. Build the host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

Output: `build/u250-host/src/host/run_saxpy`.

This step only compiles C++. It does not synthesize kernels.

---

## 8. Generate data and gold

```bash
make gen  DATASET=tiny
make gold DATASET=tiny
```

Run these once per dataset. They create `data/tiny/` with input files and the expected output.

---

## 9. Run on hardware

```bash
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

What this does:
1. Host app opens device 0 through XRT
2. Loads the xclbin
3. Finds compute unit `saxpy_1`
4. Allocates XRT buffer objects (BOs) and maps them to DDR banks
5. Copies input data from `data/tiny/` to DDR
6. Launches the kernel
7. Copies output from DDR back to host
8. Writes output to `runs/u250/hw/run_saxpy/tiny/<run_key>/`

Compare with gold:

```bash
make compare DATASET=tiny
```

**If the hardware run fails:**
- Before kernel launch: XRT, device, xclbin path, or compute unit name problem
- After kernel launch: buffer group index, data layout, kernel correctness, or result format problem

---

## 10. Software and hardware emulation

Emulation lets you test host/XRT integration without a physical card.

### When to use each mode

| | sw_emu | hw_emu |
|---|---|---|
| What it simulates | Fast C behavioral model of the kernel | RTL simulation (Vivado xsim) |
| Speed | Minutes | Hours for non-trivial data |
| Catches | Host app API bugs, BO setup, argument passing | RTL correctness, interface timing, memory protocol |
| Needs csynth first? | No | Yes |
| Dataset size | Normal sizes work | Use tiny datasets only |

### Run emulation

Generate the emulation config file (once per target):

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

Run software emulation:

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Run hardware emulation:

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Both commands set `XCL_EMULATION_MODE` and `EMCONFIG_PATH` automatically. Run outputs go under `runs/u250/<sw_emu or hw_emu>/`.

Compare emulation output with gold:

```bash
make compare DATASET=tiny
```

**If emulation fails with "cannot find emconfig":**
Check that `build/u250/emconfig/emconfig.json` exists. Run `make emconfig TARGET=u250 MODE=sw_emu` if it is missing.

**If emulation output is wrong but hardware output is correct:**
This is unusual. Check whether the `sw_emu` xclbin was actually rebuilt after a kernel change.

**If hw_emu output is wrong but sw_emu output is correct:**
The RTL has a bug that the C++ model does not catch. Debug the kernel and rerun cosim.

---

## 11. xrt.ini: runtime profiling and tracing

`config/<target>/xrt.ini` controls XRT profiling and debug output during runs. Example:

```ini
[Runtime]
verbosity = 5

[Debug]
timeline_trace = true
data_transfer_trace = coarse
```

XRT looks for this file in the same directory as the host executable. Anvil copies it during deployment. During local runs, you can copy it manually or set `XILINX_XRT_INI` to its path.

---

## 12. Stream pipeline demo (optional)

Some accelerator configs include `config/<target>/pipeline_demo.cfg` for a kernel-to-kernel stream pipeline:

```text
saxpy_stream → vadd_stream
```

Build and run it explicitly:

```bash
make csynth TARGET=u250 KERNEL=pipeline_demo
make cosim  TARGET=u250 KERNEL=pipeline_demo
make xclbin TARGET=u250   # (uses pipeline_demo.cfg when appropriate)
make build  TARGET=u250 HOST_APP=run_pipeline_demo
make hw     TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny
```

This is opt-in. Normal builds do not include it because stream pipeline synthesis/link is slow.

---

## 13. Add a new accelerator card target

To add a similar PCIe card:

1. Copy the closest existing config: `cp -r config/u250 config/my_card`
2. Edit `config/my_card/anvil.mk`:
   - Set `ANVIL_VITIS_PART` to the FPGA part number
   - Set `ANVIL_PLATFORM` to the `.xpfm` path
3. Edit `config/my_card/link.cfg`:
   - Update memory bank names to match the new platform. U50 uses HBM banks; U250 uses DDR banks; these are not interchangeable.
4. Add CMake presets for the new target to `CMakePresets.json` (copy and rename `u250-host`).
5. Add platform metadata to `tools/hlsflow/platform_info.py` so `make analyze` shows correct resource percentages.
6. Test synthesis before linking:

```bash
make csynth TARGET=my_card KERNEL=saxpy
make analyze TARGET=my_card KERNEL=saxpy
```

**Do not assume memory bank names are portable across cards.** Always check the platform documentation or an existing Vitis example for that specific card.

---

## 14. Troubleshooting accelerator card problems

### `xbutil examine` shows no card

- Driver not installed: check `dmesg | grep -i xdma` or `dmesg | grep -i xocl`
- Card not in PCIe slot correctly
- XRT version mismatch with card firmware: run `xbutil program --update`

### csynth passes but xclbin link fails

Common causes:
- `sp=` argument name does not match the kernel argument name
- Memory bank name not supported by this platform
- Too many compute units for the device resources
- Platform version mismatch between xpfm and Vitis installation

### Host app says "xclbin not found"

Check that `make xclbin TARGET=<t>` completed for the right `MODE`. The `hw` run needs the `hw` xclbin, not the `hw_emu` one.

### Host app says "kernel not found" or "CU not found"

The compute unit name in the host app (`ctx.GetKernel("saxpy:{saxpy_1}")`) does not match the instance name in `link.cfg` (`nk=saxpy:1:saxpy_1`). Fix one to match the other.

### Buffer allocation fails

Check BO group indices. The index passed to `xrt::bo` (or `kernel.group_id(arg_index)`) must correspond to the correct memory bank. Argument index 0 is the first pointer argument in the kernel signature.

### Output is wrong but no error

Check in this order:
1. BO group indices in the host app
2. Kernel argument order (does the host pass them in the same order as the C++ signature?)
3. `link.cfg` `sp=` bindings
4. Pack count vs element count (the kernel often takes `n_packs`, not raw element count)
5. Tail-lane padding (elements not divisible by pack width need special handling)
6. Dataset files — are the correct input files being read?
