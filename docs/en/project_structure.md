# Project structure

This page explains what each directory and file in the repository is for. Use it as a reference when you are navigating the codebase or adapting the template to your own project.

## Top-level files

| File | What it is |
|---|---|
| `Makefile` | The main user interface. All `make <target>` commands go here. It wraps CMake presets and adds convenience targets for HLS, deployment, analysis, etc. |
| `CMakeLists.txt` | Top-level CMake build definition. Defines the `anvil_core` interface library and conditionally includes subdirectories based on build options (`ANVIL_BUILD_*`). |
| `CMakePresets.json` | CMake presets for configure, build, and test. Each board has its own preset that sets the toolchain file, Vitis platform, and XRT paths. |
| `pyproject.toml` | Python package definition. Installs the `anvil` Python package and its test dependencies. |
| `.clang-format` | C++ code formatting rules. |

## `cmake/` — CMake modules

These are the reusable build-system pieces. Each file defines a CMake module or finder.

| File | What it does |
|---|---|
| `AnvilKernel.cmake` | Defines `add_anvil_kernel()` and `add_anvil_xclbin()` — the functions that run `v++ --compile` and `v++ --link` to turn C++ into HLS IP and link it into an xclbin. |
| `AnvilHost.cmake` | Defines `add_anvil_host()` — a thin wrapper around `add_executable()` that handles linking against the anvil libraries. |
| `ProjectOptions.cmake` | Declares the `ANVIL_BUILD_*` options (GOLD, HLS_MODEL, APPS, KERNELS, XRT, TESTS). |
| `BuildOptionValidation.cmake` | Validates that incompatible combinations of `ANVIL_BUILD_*` options are caught early. |
| `CompilerLauncher.cmake` | Integrates `ccache` or other compiler launchers. |
| `FindVitis.cmake` | Locates the Vitis installation (`v++`, headers, platform packages). |
| `FindXRT.cmake` | Locates the XRT installation (headers, libraries). |
| `Toolchain-aarch64-linux.cmake` | Cross-compilation toolchain file for AArch64 (embedded ZynqMP boards). |
| `Toolchain-x86_64-linux.cmake` | Toolchain file for x86_64 Linux host builds. |
| `parse_hls_report.py` | Python helper that wraps HLS report parsing. Used internally by the build system. |
| `anvilConfig.cmake.in` | Template for `find_package(anvil)` support after you install the library. |

## `config/<target>/` — Per-board configuration

Each board (u250, zcu102, etc.) has its own directory with device-specific files.

| File | What it is |
|---|---|
| `anvil.mk` | Makefile fragment included by the top-level `Makefile`. Sets board-specific variables like `ANVIL_PLATFORM`, `ANVIL_VITIS_PART`, `ANVIL_PRESET`, `ANVIL_KERNEL_TARGETS`, etc. |
| `link.cfg` | Vitis linker configuration. Defines connectivity (`nk=`, `sp=`) and clock (`freqHz=`) for xclbin linking. On accelerator cards that support streaming, this also sets up kernel-to-kernel connections. |
| `pipeline_demo.cfg` | Streaming pipeline configuration (accelerator cards only). Defines how saxpy_stream and vadd_stream connect as a pipeline. |
| `xrt.ini` | XRT runtime configuration for the board. Sets flags like `Runtime.xrt_profile=true` or `Debug.enable_profile=true`. |
| `README.md` | Board-specific notes: platform path, shell version, known issues. |

## `src/` — C++ source code

### `src/kernels/` — FPGA kernel sources

| File | What it is |
|---|---|
| `saxpy_kernel.cpp` | The saxpy HLS kernel (single compute unit, reads A and X, writes Y). |
| `vadd_kernel.cpp` | The vadd HLS kernel (vector addition, pair of input buffers). |
| `saxpy_stream_kernel.cpp` | Streaming version of saxpy for kernel-to-kernel pipeline demo. |
| `vadd_stream_kernel.cpp` | Streaming version of vadd for kernel-to-kernel pipeline demo. |
| `CMakeLists.txt` | Registers kernels with `add_anvil_kernel()` and xclbins with `add_anvil_xclbin()`. This is where you add your own kernels. |

### `src/host/` — XRT host programs

| File | What it is |
|---|---|
| `run_saxpy.cpp` | Host program for the saxpy kernel. Creates XRT context, allocates buffers, runs the kernel, writes output. |
| `run_vadd.cpp` | Host program for the vadd kernel. |
| `run_pipeline_demo.cpp` | Host program for the streaming pipeline demo. Runs both kernels in a chain. |
| `CMakeLists.txt` | Registers host programs with `add_anvil_host()`. This is where you add your own host apps. |

### `src/gold/` — Golden reference (CPU correctness baseline)

| File | What it is |
|---|---|
| `cpp/saxpy_gold.cpp` | C++ implementation of the saxpy golden reference (CPU-only, no XRT). |
| `cpp/saxpy_gold_main.cpp` | Command-line wrapper that reads dataset and writes gold output. |
| `cpp/metrics.cpp` | Optional metric computation used by some tests. |
| `python/` | Python golden reference implementations. |
| `CMakeLists.txt` | Defines the `anvil_gold` library and the `saxpy_gold_bin` executable. |

### `src/hls_model/` — HLS CPU model

| File | What it is |
|---|---|
| `saxpy_hls_model.cpp` | A C++ model of the saxpy kernel that mirrors the HLS implementation. Used to verify that the kernel algorithm matches the expected behavior before synthesis. |

### `src/apps/` — CLI utility programs

| File | What it is |
|---|---|
| `gen_dataset.cpp` | Generates input datasets under `data/<dataset>/`. |
| `run_gold.cpp` | Runs the golden reference and writes output for comparison. |
| `compare_gold_hls_model.cpp` | Compares gold output against the HLS model output. |

### `src/anvil/` — Core library

| File | What it is |
|---|---|
| `runtime/xrt_context.cpp` | XRT context wrapper — device selection, program loading, kernel handle creation. |
| `runtime/kernel_handle.cpp` | Kernel argument management and execution. |
| `runtime/CMakeLists.txt` | Builds `anvil_runtime` static library (links XRT). |
| `CMakeLists.txt` | Defines the `anvil_core` interface library and all sub-libraries (log, cli, json, etc.). |

## `include/anvil/` — C++ headers

Headers are organized by component. Each subdirectory has a `*.hpp` file for that component.

| Directory | What it provides |
|---|---|
| `runtime/` | `xrt_context.hpp`, `xrt_buffer.hpp`, `kernel_handle.hpp` — XRT runtime wrappers. |
| `kernels/` | `saxpy.hpp`, `vadd.hpp`, `pipeline_types.hpp` — kernel interface structs and ABI types. |
| `gold/` | `saxpy_gold.hpp`, `gold_interface.hpp` — gold reference interfaces. |
| `hls/` | `hls_types.hpp`, `data_pack.hpp`, `fixed_config.hpp`, `stream_utils.hpp` — HLS utility types. |
| `cli/` | `argparse.hpp` — command-line argument parsing wrapper. |
| `compare/` | Comparators for verifying output data (bitwise, element-wise, classification, signal). |
| `json/` | JSON serialization/deserialization. |
| `log/` | Logging wrapper (spdlog-based). |
| `table/` | Terminal table formatting. |
| `progress/` | Progress bar display (indicators-based). |
| `test/` | Test utilities. |
| `toml/` | TOML file parsing (tomlplusplus-based). |
| `config.hpp.in` | Template for generated config header (version info, build flags). |
| `platform.hpp` | Platform detection and capabilities. |
| `types.hpp` | Common type aliases. |

## `tests/` — Tests

| Directory | What it contains |
|---|---|
| `cpp/` | C++ Catch2 unit tests for compare, log, runtime, gold, and HLS model. |
| `kernels/` | HLS cosimulation testbenches (`saxpy_cosim_tb.cpp`, `vadd_cosim_tb.cpp`, etc.). |
| `python/` | Python pytest tests for comparison, gold, board_run, HLS flow parsers. |
| `data/` | Test data fixtures. |
| `install/` | Install verification smoke test. |
| `CMakeLists.txt` | Registers CTest tests (both C++ and Python). |

## `python/anvil/` — Python package

Mirrors the C++ library components for use from Python. Each module has the same name as its C++ counterpart.

| File | What it provides |
|---|---|
| `__init__.py` | Package init, version string. |
| `compare.py` | Output comparison logic (Python equivalent of C++ compare). |
| `gold.py` | Python golden reference runner. |
| `cli.py` | Python CLI argument helpers. |
| `json.py` | JSON helpers for test data. |
| `toml.py` | TOML helpers. |
| `log.py` | Python logging setup. |
| `progress.py` | Progress bar display. |
| `table.py` | Terminal table formatting. |
| `test.py` | Python test support utilities. |

## `tools/` — Developer tools

| File | What it does |
|---|---|
| `hlsflow/` | HLS flow analysis toolkit: discovers builds, parses csynth/cosim/vitis reports, runs threshold checks, compares HLS runs, generates HTML/TXT/JSONL reports. Documented in `tools/hlsflow/README.md`. |
| `sweep.py` | Clock frequency sweeping tool. Takes a TOML config file and runs multiple builds at different clock targets. |

## `scripts/` — Build and deployment scripts

| File | What it does |
|---|---|
| `gen_dataset.py` | Generates input datasets. Called by `make gen`. |
| `run_gold.sh` | Runs the golden reference. Called by `make gold`. |
| `compare.py` | Compares hardware output against golden reference. Called by `make compare`. |
| `analyze.py` | Legacy HLS report parser. Called by `make analyze-legacy`. |
| `board_run.py` | Deploys binaries to a board via SSH, runs the host app, and retrieves output. Called by `make test-xrt-hw`. |
| `emconfig.sh` | Generates `emconfig.json` for hardware emulation. Called by `make emconfig`. |

## `config/` — Per-board Makefile configuration

| Subdirectory | Board |
|---|---|
| `u250/` | Alveo U250 |
| `u50/` | Alveo U50 |
| `u55c/` | Alveo U55C |
| `u200/` | Alveo U200 |
| `u280/` | Alveo U280 |
| `vck5000/` | Versal VCK5000 |
| `zcu102/` | ZynqMP ZCU102 |
| `zcu104/` | ZynqMP ZCU104 |
| `zcu106/` | ZynqMP ZCU106 |
| `kv260/` | Kria KV260 |
| `README.md` | Explains how to add a new board. |

## `data/` — Datasets

| Subdirectory | Contents |
|---|---|
| `tiny/` | Small demo dataset. Default for `DATASET=tiny`. |
| `empty/` | Empty dataset for edge-case testing. |
| `badshape/` | Dataset with mismatched dimensions for error-path testing. |
| `task15_smoke/` through `task16_ok/` | Test datasets used by specific test scenarios. |

## `reports/` — HLS analysis output

Generated by `make analyze-flow` and `make analyze-cosim`. Contains:

- `reports/<run_id>.html` / `reports/<run_id>.txt` — per-run HLS reports
- `reports/runs.jsonl` — JSONL database of all HLS runs

## `third_party/` — Vendored dependencies

| Library | What it is used for |
|---|---|
| `argparse/` | Command-line argument parsing (header-only). |
| `catch2/` | C++ unit test framework (Catch2 amalgamated header). |
| `hlslib/` | HLS utility library (simulation helpers, dataflow patterns). |
| `indicators/` | Terminal progress bars. |
| `nlohmann/` | JSON serialization (`nlohmann/json`). |
| `spdlog/` | Logging framework. |
| `tabulate/` | Terminal table formatting. |
| `tomlplusplus/` | TOML configuration file parsing. |

## `platforms/` — Board platform metadata

Board descriptions in TOML format. Used by Python tools to look up platform properties.

| Subdirectory | Board |
|---|---|
| `zcu102/` | ZCU102 board metadata. |
| `kv260/` | KV260 board metadata. |

## `docs/` — Documentation

| Path | Contents |
|---|---|
| `en/` | English documentation. |
| `zh/` | Chinese documentation. |
| `superpowers/` | Design documents and specifications. |
| `assets/` | Images and banners. |

## How the build phases map to directories

| Phase | What is built | Key CMake option | Source directory |
|---|---|---|---|
| Gold reference | CPU correctness baseline | `ANVIL_BUILD_GOLD` | `src/gold/` |
| HLS model | CPU simulation of kernel | `ANVIL_BUILD_HLS_MODEL` | `src/hls_model/` |
| Utility apps | Dataset gen, gold runner, compare | `ANVIL_BUILD_APPS` | `src/apps/` |
| FPGA kernels | HLS synthesis | `ANVIL_BUILD_KERNELS` | `src/kernels/` |
| XRT host | Host programs with XRT | `ANVIL_BUILD_XRT` | `src/host/` |
| Tests | Unit tests, cosim, Python tests | `ANVIL_BUILD_TESTS` | `tests/` |
