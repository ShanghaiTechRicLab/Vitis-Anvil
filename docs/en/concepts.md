# Concepts: what the flow is doing

This page explains the vocabulary used throughout these docs. Read it once before you run your first command. Experts can skim for the terms they don't know yet.

## The one-sentence version

An FPGA accelerator project has a CPU program (host app), an FPGA program (kernel), and a compile chain (Vitis) that turns C++ into FPGA hardware. Vitis-Anvil adds the project structure, build rules, and test ladder around those three pieces.

---

## Two programs, one binary

Every FPGA accelerator has two programs running at once:

**Kernel** — C++ that Vitis HLS compiles into FPGA hardware. It runs on the FPGA fabric.

**Host app** — Normal C++ that runs on the CPU. It loads the FPGA binary into the device, allocates memory buffers, copies data to the FPGA, starts the kernel, and reads results back.

Between them is the **xclbin**: the FPGA binary file produced by Vitis. The host app loads it at runtime through XRT.

```text
[CPU: host app]  ──XRT──→  [xclbin loaded onto FPGA]  ──on-chip memory──→  results
```

The development flow works model-first to catch bugs cheaply before running slow FPGA tools:

```text
gold → hls_model → csynth → cosim → xclbin → swemu/hwemu/qemu/hw
```

---

## The four Make selectors

Most commands accept these variables. Get these right before anything else.

| Variable | Selects | Example values |
|---|---|---|
| `TARGET` | Board or platform config from `config/<target>/anvil.mk` | `u250`, `u55c`, `zcu102`, `kv260` |
| `KERNEL` | HLS kernel target name | `saxpy`, `vadd`, `pipeline_demo`, `all` |
| `HOST_APP` | CPU host executable to build or run | `run_saxpy`, `run_vadd` |
| `DATASET` | Input dataset directory under `data/` | `tiny`, `my_dataset` |

Example:

```bash
make csynth TARGET=u250 KERNEL=saxpy
make hw     TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Do not mix them. `HOST_APP` does not select which kernel to synthesize. `KERNEL` does.

---

## MODE: what environment are you building for?

`MODE` is a fifth selector that controls which Vitis output to build and which XRT environment to run in.

| MODE | Environment | Needs Vitis? | Needs XRT? | Needs physical card? |
|---|---|:---:|:---:|:---:|
| `hw` | Real FPGA hardware | to build xclbin | yes at runtime | yes |
| `hw_emu` | Hardware emulation — RTL simulation | yes | yes | no |
| `sw_emu` | Software emulation — fast C model | yes | yes | no |
| `qemu` | Embedded QEMU (ZynqMP/MPSoC) | yes | on board | no |

`MODE` defaults to `hw`. Pass it explicitly when building for emulation:

```bash
make csynth  TARGET=u250 MODE=hw_emu KERNEL=saxpy
make xclbin  TARGET=u250 MODE=hw_emu HOST_APP=run_saxpy
make emconfig TARGET=u250 MODE=sw_emu
```

Run commands fix the mode by their name so you do not need to pass MODE separately:

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # always MODE=sw_emu
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # always MODE=hw_emu
make hw    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny   # always MODE=hw
make qemu  TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny # always MODE=qemu
```

CPU-only stages (`test`, `gold`, HLS model) ignore `TARGET` and `MODE` entirely.

### sw_emu vs hw_emu: which one to use?

| | sw_emu | hw_emu |
|---|---|---|
| What runs | The original C++ kernel compiled for x86_64 | RTL simulation using the synthesized hardware (Vivado xsim) |
| Speed | Fast: minutes | Slow: hours for large data |
| Catches | Host/XRT API bugs, buffer setup, argument passing | RTL correctness, interface timing, memory protocol |
| Needs csynth? | No (skips synthesis) | Yes (must synthesize the kernel first) |
| Dataset size | Can use normal sizes | Use tiny datasets; large datasets make simulation impractical |
| Use it for | Quick host/XRT smoke test | Verifying the synthesized RTL before committing to hardware |

In practice: run `swemu` first to confirm the host app is correct, then run `hwemu` to confirm the synthesized RTL is correct, then run `hw` on real hardware.

---

## Glossary

### Kernel

A kernel is a C/C++ function that Vitis HLS turns into FPGA hardware. Project kernels live in:

```text
src/kernels/*.cpp                          ← Vitis top: HLS pragmas + dataflow region
src/kernels/include/kernels/*.hpp          ← Shared headers: ABI, types, core helpers
```

The header files are important because the same function signature and pack types must be shared by the kernel, its cosim testbench, and the host app.

### Host app

A host app runs on the CPU and uses XRT to:

1. Open the FPGA device
2. Load the xclbin
3. Allocate input/output buffer objects (BOs)
4. Copy input data to the device
5. Launch the kernel
6. Copy output data back
7. Write results

Host apps live in `src/host/*.cpp`. Examples: `run_saxpy`, `run_vadd`, `run_pipeline_demo`.

### Gold reference

A gold reference is the simplest correct CPU implementation of the math. It is the truth model. It should be easy to read, not optimized.

Gold code lives in `src/gold/**`. Use it to answer: "What should the FPGA output be?"

### HLS model

An HLS model is CPU code that mirrors the kernel's structure more closely than the gold reference. It is useful when your kernel uses packed vectors, streams, or dataflow, because you can catch those-specific bugs without running Vitis.

HLS model code lives in `src/hls_model/**`.

The HLS model **does not prove** timing, resource use, Vitis linking, XRT correctness, or board behavior. It is a CPU simulation only.

### Kernel core

A kernel core is project-owned HLS-compatible code shared by both the HLS model and the Vitis kernel top. For `saxpy`:

```text
src/kernels/include/kernels/saxpy_core.hpp
```

It contains the Load/Compute/Store stage helpers and the `SaxpyOp` functor. It has no host code, XRT code, or board configuration.

The kernel calls these helpers through `ANVIL_DATAFLOW_*` macros. The HLS model calls the identical helpers from normal CPU C++.

### Kernel ABI header

The kernel ABI header declares the `extern "C"` kernel signature. It is the contract between the kernel, its cosim testbench, and the host app. Example:

```text
src/kernels/include/kernels/saxpy_kernel.hpp
```

### Kernel pack types

Two files split the type definitions for maximum portability:

- **`abi.hpp`** — Pack width constants only (`kSaxpyPackWidth = 16`). No Vitis or hlslib includes. Safe for cross-compiled host builds.
- **`kernel_types.hpp`** — Pack typedefs using `anvil::hls::Pack<T, N>`. Includes HLS headers. For kernel and model code only.

This split is important for embedded host cross-compiles: `abi.hpp` can be included safely without pulling in HLS synthesis libraries.

### csynth (C synthesis)

`csynth` is HLS C synthesis: Vitis reads your kernel C++ and generates hardware-level reports plus a compiled kernel object (`.xo`).

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

What csynth tells you:

- **Did Vitis accept the code?** — Some C++ patterns are not synthesizable. This is the first place you find out.
- **Initiation Interval (II)** — How many clock cycles between successive inputs. II=1 is ideal: the pipeline accepts one new input per clock cycle. II=2 means half the throughput. The `#pragma HLS pipeline II=1` pragma requests II=1; the actual result depends on data dependencies and memory access patterns.
- **Resource estimates** — LUT, FF, DSP, BRAM, URAM utilization. These are pre-place-and-route estimates; final numbers differ slightly after implementation.
- **Estimated clock period** — Whether the design can meet timing at your target clock.

**Resource types:**

| Resource | What it is | When your kernel uses more |
|---|---|---|
| LUT | Look-up tables: combinational logic | Mux-heavy control, wide operations |
| FF | Flip-flops: pipeline registers | More pipeline stages, wider data paths |
| DSP | Multiply-accumulate blocks | FP or integer multiply operations |
| BRAM | Block RAM (18 Kb units) | Internal buffers, FIFOs |
| URAM | Ultra RAM (288 Kb units, newer devices) | Large lookup tables, deep queues |
| HBM | High Bandwidth Memory (on HBM cards, e.g. U50) | HBM port binding in link.cfg |

Read csynth results with:

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

### cosim (C/RTL cosimulation)

`cosim` runs a C++ testbench against the RTL that Vitis generated from your kernel. It verifies that the generated RTL behaves exactly as the C++ kernel specified.

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

Cosim is **not** a host app run. It does not load an xclbin. It does not use XRT. It is a kernel-level RTL test.

If cosim fails, fix the kernel or testbench before proceeding to xclbin link or host runs.

### xclbin

An xclbin is the FPGA binary loaded by XRT. The Vitis linker produces it from kernel objects (`.xo`) plus a platform file and connectivity config.

```bash
make xclbin TARGET=u250
```

The linker uses:

1. Kernel objects from synthesis (e.g. `saxpy.xo`)
2. The platform `.xpfm` file (device-specific)
3. `config/<target>/link.cfg` — connectivity rules

One xclbin per target/mode. `sw_emu`, `hw_emu`, and `hw` builds produce different xclbins.

### platform / xpfm

A platform file (`.xpfm`) describes what board or card Vitis is building for: the device part, available clocks, memory interfaces, shell logic, and supported build flows.

Each target config points to one platform file. For U250:

```make
# config/u250/anvil.mk
ANVIL_PLATFORM := /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
```

If this path is wrong, every Vitis step fails immediately. Always verify the platform path before running synthesis.

### XRT

XRT (Xilinx Runtime) is the library host apps use to talk to the FPGA. It provides the C++ API for opening devices, loading xclbins, allocating buffer objects, launching kernels, and synchronizing data.

XRT installation provides:
- Headers: `/opt/xilinx/xrt/include/`
- Libraries: `/opt/xilinx/xrt/lib/`
- Setup script: `. /opt/xilinx/xrt/setup.sh`

Source XRT before building host apps or running hardware:

```bash
. /opt/xilinx/xrt/setup.sh
xbutil examine   # should list any installed FPGA cards
```

### Buffer objects (BOs) and group_id

A buffer object (BO) is a piece of host-accessible memory that XRT manages. The host app allocates BOs, fills them with input data, syncs them to the device, launches the kernel, syncs output back, and reads the results.

BOs must be mapped to the correct memory bank (DDR or HBM). The bank is determined by the kernel argument position and the `link.cfg` connectivity:

```cpp
// Argument index 0 is `x` in kernel, mapped to DDR[0] by link.cfg
auto xbuf = xrt::bo(device, size_bytes, kernel.group_id(0));
```

`kernel.group_id(arg_index)` automatically reads the memory bank assignment from the xclbin.

**BO group indices match the order of pointer arguments in the kernel signature.** Scalar arguments (like `alpha` or `n_packs`) are not BOs and have no group_id.

### emconfig.json

`emconfig.json` is a runtime configuration file required by XRT when running in software or hardware emulation. It tells XRT what platform to emulate.

Generate it with:

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

Anvil stores it at `build/<target>/emconfig/emconfig.json`. One file works for both `sw_emu` and `hw_emu` on the same target.

When running emulation, `make swemu` and `make hwemu` set `EMCONFIG_PATH` automatically. If you run the host app manually in emulation mode, set:

```bash
XCL_EMULATION_MODE=sw_emu EMCONFIG_PATH=build/u250/emconfig ./run_saxpy ...
```

Missing or wrong `EMCONFIG_PATH` causes XRT to fail before opening the device.

### link.cfg

`link.cfg` is a Vitis linker configuration file that tells Vitis how to connect kernels to memory banks. It lives at `config/<target>/link.cfg`.

Full format:

```ini
[connectivity]
# Create compute units: nk=<top_function>:<count>:<instance_name>
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1

# Bind kernel pointer arguments to memory banks:
# sp=<instance_name>.<argument_name>:<memory_bank>
# Scalar arguments (alpha, n_packs, etc.) are NOT listed here.
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]
sp=vadd_1.a:DDR[0]
sp=vadd_1.b:DDR[1]
sp=vadd_1.out:DDR[2]

[clock]
# Request target clock: freqHz=<frequency_in_Hz>:<instance_name>
freqHz=300000000:saxpy_1
freqHz=300000000:vadd_1
```

Important rules:
- `nk=` names must match the C++ `extern "C"` function name.
- `sp=` argument names must match the kernel function argument names exactly.
- Scalar arguments (`alpha`, `beta`, `n_packs`, etc.) are control register arguments — they do NOT need `sp=` entries.
- Memory bank names (`DDR[0]`, `HBM[0:3]`) must match what the platform provides. Different cards have different bank naming.
- The host app must open kernels by the instance name from `nk=`, e.g. `saxpy:{saxpy_1}`.

### xrt.ini

`xrt.ini` is a runtime configuration file that enables XRT profiling and debugging. It lives at `config/<target>/xrt.ini`. Typical contents:

```ini
[Runtime]
verbosity = 5

[Debug]
timeline_trace = true
data_transfer_trace = coarse
```

Copy it to the same directory as the host executable before running.

### Dataset

A dataset is an input/reference directory:

```text
data/<name>/
  meta.json        ← size, dtype, and parameters used to generate this case
  x.bin            ← binary input array
  y.bin            ← binary input array
  gold_out.bin     ← expected output (written by make gold)
```

Generate a dataset:

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

Run outputs go under `runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/`, not under `data/`. The `data/` directory is input-only.

---

## Saxpy file map: one complete example

The demo `saxpy` kernel is intentionally split by responsibility:

```text
src/gold/cpp/saxpy_gold.cpp                          ← CPU truth implementation
src/kernels/include/kernels/abi.hpp                  ← pack-width constants, host-safe
src/kernels/include/kernels/kernel_types.hpp         ← SaxpyPack typedef
src/kernels/include/kernels/saxpy_kernel.hpp         ← extern "C" ABI declaration
src/kernels/include/kernels/saxpy_core.hpp           ← Load/Compute/Store + SaxpyOp
src/kernels/saxpy_kernel.cpp                         ← Vitis top: HLS pragmas + dataflow
src/hls_model/saxpy_hls_model.cpp                   ← CPU model, mirrors kernel structure
src/host/run_saxpy.cpp                              ← XRT host app
```

- `saxpy_gold.cpp` computes the mathematically correct output on CPU. Simple scalar loop.
- `abi.hpp` defines `kSaxpyPackWidth` without any HLS headers. Safe to include everywhere.
- `kernel_types.hpp` defines `SaxpyPack = anvil::hls::Pack<float, kSaxpyPackWidth>`.
- `saxpy_kernel.hpp` declares the `extern "C"` signature shared by kernel, cosim, and host.
- `saxpy_core.hpp` holds the Load/Compute/Store non-templated wrappers and `SaxpyOp` functor. Both the kernel and the HLS model call these.
- `saxpy_kernel.cpp` is the Vitis top: it sets `#pragma HLS INTERFACE`, instantiates streams, and calls the core through `ANVIL_DATAFLOW_*`.
- `saxpy_hls_model.cpp` packs scalar floats, calls the same core helpers, and unpacks results. CPU simulation.
- `run_saxpy.cpp` opens the device, loads the xclbin, allocates BOs, runs the kernel, and writes results.

---

## hlslib framework helpers

Vitis-Anvil wraps selected hlslib primitives under `include/anvil/hls/` so kernel code uses one consistent namespace.

| Header | Provides |
|---|---|
| `pack.hpp` | `anvil::hls::Pack<T,N>`, `PackTraits`, `GetLane`, `SetLane` |
| `stream.hpp` | `anvil::hls::Stream<T,Depth>`, `kDefaultStreamDepth`, `kDefaultDataflowStreamDepth` |
| `dataflow.hpp` | `ANVIL_DATAFLOW_INIT`, `ANVIL_DATAFLOW_FUNCTION`, `ANVIL_DATAFLOW_FINALIZE` macros |
| `packed_ops.hpp` | `LoadPacks`, `StorePacks`, `MapPacks`, `MapPacksWithScalar`, `MapMem2Packs` |
| `axis.hpp` | `WriteAxis`, `ReadAxis` — compatibility helpers for AXI stream ports |

These are framework code. Include them from your kernels; do not modify them.

Your project kernel headers go under `src/kernels/include/kernels/`. Do not add your kernel types to `include/anvil/`.

---

## Code boundary: framework vs project

```text
include/anvil/**         ← Framework. Do not add your types here.
src/anvil/**             ← Framework implementation. Do not edit unless fixing a bug.

src/kernels/**           ← Your kernel code. Edit freely.
src/kernels/include/**   ← Your kernel ABI headers and shared cores.
src/hls_model/**         ← Your HLS CPU models.
src/gold/**              ← Your golden reference.
src/host/**              ← Your XRT host applications.
config/**                ← Your board/target configurations.
```

---

## HLS pragma reference

These pragmas appear in kernel `.cpp` files inside the Vitis top function body. They are instructions to the HLS synthesizer, not executable code.

### Interface pragmas

```cpp
// AXI master port: for pointer arguments (DDR/HBM memory access)
#pragma HLS INTERFACE m_axi port=x bundle=gmem0 offset=slave depth=1024

// AXI lite port: for scalar arguments and the return value (control)
#pragma HLS INTERFACE s_axilite port=x       bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control
```

Rules:
- Every **pointer argument** needs an `m_axi` pragma. The `bundle=gmem0` names the AXI master interface; match it to the memory bank via `link.cfg`.
- Every **scalar argument** and the `return` needs an `s_axilite` pragma with `bundle=control`.
- `depth=1024` is a simulation hint for cosim. Set it to the maximum number of elements (not packs) the port will access.

### Pipeline pragma

```cpp
#pragma HLS pipeline II=1
```

Place inside a loop. Requests initiation interval 1 — one new loop iteration started per clock cycle. The HLS tool achieves II=1 when there are no loop-carried dependencies or memory port conflicts.

### Dataflow pragma

```cpp
#pragma HLS dataflow
```

Enables task-level pipelining across function calls. Requires that functions communicate only through streams or `#pragma HLS stream` variables. The `ANVIL_DATAFLOW_*` macros wrap this pattern.

---

## Where to go next

- **New user**: read [Get started](get_started.md) for a complete first-run walkthrough.
- **Adding your own algorithm**: read [Customization guide](customization.md).
- **Accelerator card (U250, U50, U55C, ...)**: read [Accelerator-card flow](accelerator_flow.md).
- **Embedded board (ZCU102, KV260, ...)**: read [Embedded flow](embedded_flow.md).
- **hlslib kernel skeleton**: read [hlslib adaptation](hlslib_adaptation.md).
- **Understanding the build caching model**: read [Build system model](build_system.md).
