# Project structure: where each piece lives

This page explains the repository layout from the point of view of someone building an FPGA accelerator. The main idea is separation: framework code, kernel code, host code, data code, and board configuration should not be mixed.

## The top-level directories

| Path | What it is | Why it exists |
|---|---|---|
| `include/anvil/` | Public framework headers | Generic helpers that are installed/exported for reuse |
| `src/anvil/` | Framework implementation | Runtime/logging/etc. implementation used by apps |
| `src/kernels/` | HLS kernel implementations | Code that Vitis HLS turns into hardware |
| `src/kernels/include/kernels/` | Project kernel ABI headers | Shared declarations for kernels, host apps, and testbenches |
| `src/host/` | XRT host apps | CPU programs that load xclbin and launch kernels |
| `src/gold/` | CPU truth/reference code | Simple correct implementation used for comparison |
| `src/hls_model/` | CPU-compiled HLS-style model | Checks packed/stream algorithm behavior before synthesis |
| `src/apps/` | Utility CLIs | Dataset generation, gold runner, comparison runner |
| `tests/` | C++/Python/cosim/install tests | Verifies each layer before hardware |
| `config/` | Per-board configuration | xpfm path, part, xrt.ini, link.cfg, Make variables |
| `tools/` | Python analysis tools | HLS report parsing, comparison, summaries |
| `scripts/` | Shell/Python flow helpers | Board run, dataset helpers, legacy wrappers |
| `docs/` | User documentation | How to use and adapt the template |
| `build/` | Generated build trees | Created by CMake; do not edit or commit |
| `data/` | Generated datasets | Inputs and reference outputs for examples |
| `runs/` | Runtime outputs | Per-target/mode/host/dataset run artifacts |
| `reports/` | Generated analysis reports | HTML/TXT/JSONL summaries from hlsflow |

## Framework code vs project code

Framework code is the Anvil infrastructure. Most users should not edit it:

```text
include/anvil/**
src/anvil/**
```

Project code is where you add your accelerator:

```text
src/kernels/**
src/kernels/include/kernels/**
src/hls_model/**
src/gold/**
src/host/**
config/**
tests/**
```

Why this matters: if you put your algorithm under `include/anvil/`, it becomes part of the framework API and may be installed for downstream consumers. Kernel-specific types such as `ScaleAddPack` should not be framework API.

## Kernel files

A typical kernel has three files:

```text
src/kernels/include/kernels/my_kernel.hpp   # ABI and type declarations
src/kernels/my_kernel.cpp                   # HLS implementation
tests/kernels/my_kernel_cosim_tb.cpp        # C/RTL cosim testbench
```

The header declares the top function:

```cpp
extern "C" void my_kernel(...);
```

The `.cpp` implements it with HLS pragmas. The testbench calls the same function with normal C++ arrays/vectors.

## Host app files

Host apps live in:

```text
src/host/run_saxpy.cpp
src/host/run_vadd.cpp
src/host/run_pipeline_demo.cpp
```

A host app is not synthesized. It runs on CPU and uses XRT. It needs to know:

- xclbin path
- kernel compute-unit name, such as `saxpy:{saxpy_1}`
- buffer argument order
- dataset input/reference file names and runtime output path

Host executables are registered in `src/host/CMakeLists.txt`.

## Board config files

Each target has a directory:

```text
config/u250/
config/zcu102/
```

Important files:

| File | Meaning |
|---|---|
| `anvil.mk` | Make variables: platform path, preset names, target type, kernel target list |
| `link.cfg` | Vitis linker connectivity: compute units, DDR/HBM banks, clocks |
| `pipeline_demo.cfg` | Optional stream pipeline connectivity |
| `xrt.ini` | XRT runtime tracing/debug settings |
| `README.md` | Notes specific to that board or platform |

`TARGET=u250` means “load `config/u250/anvil.mk` and use the presets/platform described there.”

## Build directories

CMake writes generated files under `build/<preset>/`. Examples:

```text
build/hls-model-linux-debug/
build/gold-linux-debug/
build/u250-host/
build/zcu102-kernel/
build/zcu102-host/
```

Do not edit files under `build/`. If generated files look wrong, fix the source CMake/config files and reconfigure.

## Data and reports

Dataset directories contain inputs and reference outputs:

```text
data/tiny/meta.json
data/tiny/x.bin
data/tiny/y.bin
data/tiny/gold_out.bin
```

Hardware/emulation run outputs are kept separately, for example:

```text
runs/u250/hw_emu/run_saxpy/tiny/latest/out.bin
runs/u250/hw_emu/run_saxpy/tiny/latest/run.json
runs/u250/hw_emu/run_saxpy/tiny/latest/stdout.log
```

Report directories contain analysis artifacts:

```text
reports/*.html
reports/*.txt
reports/runs.jsonl
reports/runs.csv
```

Data and reports are outputs of the flow, not source code.

## Where to edit for common tasks

| Goal | Edit these files first |
|---|---|
| Add a new kernel | `src/kernels/include/kernels/*.hpp`, `src/kernels/*.cpp`, `src/kernels/CMakeLists.txt` |
| Add cosim | `tests/kernels/*_cosim_tb.cpp`, `src/kernels/CMakeLists.txt` |
| Add host app | `src/host/*.cpp`, `src/host/CMakeLists.txt` |
| Add gold reference | `src/gold/include/gold/*.hpp`, `src/gold/cpp/*.cpp`, tests |
| Add dataset format | `src/apps/gen_dataset.cpp` or `scripts/gen_dataset.py`, host/gold/compare code |
| Add board | `config/<target>/`, `CMakePresets.json`, optional `platforms/` metadata |
| Improve reports | `tools/hlsflow/**` |
| Change framework runtime | `include/anvil/runtime/**`, `src/anvil/runtime/**` |

If you are adding your own accelerator, start with [Customization guide](customization.md).
