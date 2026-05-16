# Concepts: what the flow is doing

This page is for readers who only know the basic idea: FPGA hardware can be generated from C/C++ with HLS. Vitis-Anvil adds the missing project structure around that idea.

## The short story

An FPGA application has two programs:

1. **Kernel** — the function that becomes hardware on the FPGA.
2. **Host app** — the CPU program that loads the FPGA bitstream, allocates buffers, copies data, starts the kernel, and reads results back.

Between those two programs is an FPGA binary:

- **xclbin** — the file produced by Vitis that contains the compiled kernel hardware and connectivity information.

A normal development flow is:

```text
write CPU truth model
  ↓
write HLS kernel C++
  ↓
run HLS synthesis (csynth)
  ↓
run C/RTL cosimulation (cosim)
  ↓
link .xclbin
  ↓
build host app
  ↓
run on card/board
  ↓
compare output with CPU truth
```

Vitis-Anvil gives each step a fixed place in the repository and a Make target.

## Important terms

### Kernel

A kernel is a C/C++ function that Vitis HLS turns into hardware. In this project kernels live in:

```text
src/kernels/*.cpp
src/kernels/include/kernels/*.hpp
```

The `.hpp` file is important because the same function signature must be visible to:

- the kernel implementation
- the cosim testbench
- sometimes the host app, if it needs the same pack width or ABI constants

### Host app

A host app runs on the CPU, not the FPGA. It uses XRT to:

1. open the FPGA device
2. load the xclbin
3. allocate input/output buffers
4. copy data to the device
5. launch a kernel
6. copy output back

Host apps live in:

```text
src/host/*.cpp
```

Examples are `run_saxpy`, `run_vadd`, and `run_pipeline_demo`.

### Gold reference

A gold reference is the simplest CPU implementation of the math. It is the truth model. It should be easy to read, not optimized.

Gold code lives in:

```text
src/gold/**
```

You use it to answer: “What should the FPGA output be?”

### HLS model

An HLS model is CPU code that looks more like the HLS kernel than the gold reference. It is useful when your kernel uses packed vectors, streams, or dataflow.

HLS model code lives in:

```text
src/hls_model/**
```

You use it to catch algorithm and data-layout errors before running Vitis synthesis.

### Dataset

A dataset is a directory of input/output files. It normally includes:

```text
data/<name>/meta.json
data/<name>/x.bin
data/<name>/y.bin
data/<name>/gold_out.bin
data/<name>/xrt_hw_out.bin
```

The exact files depend on your kernel. The key point is that generator, gold, host app, and compare tool must all agree on the file names and meanings.

### csynth

`csynth` means HLS C synthesis. Vitis reads your kernel C++ and produces reports plus a compiled kernel object.

It answers questions like:

- Did Vitis accept my HLS code?
- What initiation interval (II) did it achieve?
- How many LUT/FF/DSP/BRAM resources does it estimate?
- What clock does it estimate?

Run it with:

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

### cosim

`cosim` means C/RTL cosimulation. Vitis runs a C++ testbench against generated RTL to check that the RTL behaves like the C++ kernel.

It is not a host-app run. It does not load an xclbin. It is still a kernel-level test.

Run it with:

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

### xclbin

An xclbin is the binary loaded by XRT. It is produced after HLS synthesis by the Vitis linker.

The linker uses:

- kernel objects from synthesis
- the platform `.xpfm`
- `link.cfg` connectivity rules

Run it with:

```bash
make xclbin TARGET=u250
```

### platform / xpfm

A platform file (`.xpfm`) tells Vitis what board/card it is building for: device part, clocks, memory interfaces, shells, and supported flows.

Each target config points to one platform:

```text
config/u250/anvil.mk
config/zcu102/anvil.mk
```

### XRT

XRT is the runtime library used by host apps to talk to the FPGA. If the host app fails to compile because XRT headers or libraries are missing, your HLS kernel might still be fine; the CPU-side runtime environment is the problem.

### TARGET, KERNEL, HOST_APP, DATASET

These Make variables select different things:

| Variable | Selects | Example |
|---|---|---|
| `TARGET` | board/platform config | `u250`, `zcu102` |
| `KERNEL` | HLS kernel target | `saxpy`, `vadd`, `all` |
| `HOST_APP` | CPU host executable | `run_saxpy` |
| `DATASET` | data directory | `tiny` |

Do not mix them. `HOST_APP` does not select cosim. `KERNEL` does.

## Where to go next

- New user: read [Get started](get_started.md).
- Want to add your own algorithm: read [Customization guide](customization.md).
- Using an accelerator card: read [Accelerator-card flow](accelerator_flow.md).
- Using an embedded board: read [Embedded flow](embedded_flow.md).
