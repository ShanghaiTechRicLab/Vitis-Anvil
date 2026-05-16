# Adapt Vitis-Anvil to your own project

There are two common ways to use Vitis-Anvil.

## Option A: Use it as a project template

1. Copy the repository.
2. Keep `cmake/`, `CMakePresets.json`, `Makefile`, `config/`, `tools/hlsflow/`, and `python/anvil/`.
3. Replace demo kernels and host apps.
4. Keep the existing tests until your replacement tests pass.
5. Rename product-facing binaries and docs after the flow is stable.

Recommended replacement order:

```text
kernel C++ → cosim testbench → CPU model/gold → host app → dataset → compare → xclbin connectivity → board run
```

## Option B: Vendor the build modules into an existing repo

Copy these pieces into your existing project:

```text
cmake/AnvilKernel.cmake
cmake/AnvilHost.cmake
cmake/FindVitis.cmake
cmake/FindXRT.cmake
cmake/Toolchain-*.cmake
config/<target>/
tools/hlsflow/
```

Then include the modules from your top-level `CMakeLists.txt` and register your kernels with `add_anvil_kernel()` and `add_anvil_xclbin()`.

## Minimal CMake shape

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_accel LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(AnvilKernel)
include(AnvilHost)

add_subdirectory(src/kernels)
add_subdirectory(src/host)
```

## Minimal Make interface

Keep a small stable user surface:

```bash
make test
make build TARGET=<board> HOST_APP=<app>
make csynth TARGET=<board> KERNEL=<kernel>
make cosim TARGET=<board> KERNEL=<kernel>
make xclbin TARGET=<board>
make run-host TARGET=<board> HOST_APP=<app>
```

This surface is intentionally easy to wrap later with commands such as:

```bash
anvil init
anvil build --target u250 --host-app run_saxpy
anvil csynth --target u250 --kernel saxpy
```

## Naming and packaging

Use `anvil` for the internal template/tooling layer. Use your product name for the final accelerator, bitstream package, and user-facing application. This keeps the reusable build flow separate from product identity.

## What to keep from this repo

Keep:

- target split: `TARGET`, `KERNEL`, `HOST_APP`, `DATASET`
- explicit Python environment target
- explicit long-running HLS/xclbin targets
- HLS report database
- platform path override via `ANVIL_PLATFORM=`
- embedded sysroot override via `PETALINUX_SYSROOT=`

Replace:

- demo kernels
- demo host apps
- dataset format
- gold and compare logic
- connectivity files
- product documentation
