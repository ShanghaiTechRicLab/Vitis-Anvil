# Adapt Vitis-Anvil to your own project

There are two common ways to use Vitis-Anvil once you outgrow the demo.

## Option A: Use it as a project template

1. Copy the repository.
2. Keep `cmake/`, `CMakePresets.json`, `Makefile`, `config/`, `tools/hlsflow/`, and `python/anvil/`.
3. Replace the demo kernels and host apps.
4. Keep the existing tests passing until your replacement tests are ready.
5. Rename the product-facing binaries and documentation once the flow is stable.

A good replacement order:

```text
kernel C++ → cosim testbench → CPU model / gold → host app → dataset → compare → xclbin connectivity → board run
```

## Option B: Vendor the build modules into an existing repository

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

Then include the modules from your top-level `CMakeLists.txt` and use `add_anvil_kernel()` and `add_anvil_xclbin()` to register your own kernels.

## Minimal CMake structure

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

Keep a small, stable set of user-facing commands:

```bash
make test
make build TARGET=<board> HOST_APP=<app>
make csynth TARGET=<board> KERNEL=<kernel>
make cosim TARGET=<board> KERNEL=<kernel>
make xclbin TARGET=<board>
make run-host TARGET=<board> HOST_APP=<app>
```

This surface is designed so you can wrap it later:

```bash
anvil init
anvil build --target u250 --host-app run_saxpy
anvil csynth --target u250 --kernel saxpy
```

## Naming and packaging

Use `anvil` for the internal template and tooling layer. Use your own product name for the final accelerator, bitstream package, and end-user application. This keeps the reusable build flow separate from product identity.

## What to keep and what to replace

Keep:

- The target split: `TARGET`, `KERNEL`, `HOST_APP`, `DATASET` as separate concerns
- The explicit Python environment target
- Explicit long-running targets for HLS and xclbin
- The HLS report database
- Platform path override via `ANVIL_PLATFORM=`
- Embedded sysroot override via `PETALINUX_SYSROOT=`

Replace:

- Demo kernels
- Demo host apps
- Dataset format
- Golden reference and comparison logic
- Connectivity files
- Product-facing documentation
