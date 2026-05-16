# Adapt Vitis-Anvil to your own project

This page is about turning the template into a real repository for your accelerator. Read [Customization guide](customization.md) first if you have not added a kernel before.

## 1. Decide what you keep

Most projects keep:

- top-level Makefile flow
- CMake presets and helper modules
- `include/anvil/**` framework helpers
- `src/anvil/**` runtime/logging libraries
- `tools/hlsflow/**` report tools
- docs structure

Most projects replace:

- demo kernels in `src/kernels/`
- demo host apps in `src/host/`
- demo gold reference in `src/gold/`
- demo datasets and compare logic
- board configs under `config/`

## 2. Rename the problem, not the framework

Do not rename every `anvil` namespace immediately. Keep the framework stable and add your project code around it.

Good first step:

```text
src/kernels/include/kernels/my_algorithm.hpp
src/kernels/my_algorithm_kernel.cpp
src/host/run_my_algorithm.cpp
src/gold/include/gold/my_algorithm_gold.hpp
```

Bad first step:

```text
rename include/anvil to include/my_company
rewrite runtime wrappers before running a kernel
```

Renaming the framework early creates many errors without improving your hardware.

## 3. Replace demos in layers

Recommended order:

1. Keep `saxpy` working.
2. Add your new kernel next to it.
3. Add the gold reference and HLS model for the new kernel.
4. Add your host app next to existing host apps.
5. Add your dataset/gold/compare flow.
6. Add your board config.
7. Only remove demos after your flow works.

This keeps a known-good reference while you are debugging your own code.

For m_axi-style packed kernels, use the ladder:

```text
gold -> hls_model -> csynth -> cosim -> xclbin -> host
```

Keep the kernel core under `src/kernels/include/kernels/**` and share it between `src/hls_model/**` and the Vitis top in `src/kernels/*.cpp`. The HLS model catches packed layout and tail bugs early, but it does not replace synthesis, cosim, xclbin link, or host/XRT testing.

## 4. Define your project contract

Write down:

- input files
- output files
- metadata JSON fields
- kernel argument order
- pack width
- target boards
- acceptable error thresholds
- expected performance checks

Put this in your docs before the project grows. Many FPGA bugs are really contract mismatches between host, kernel, and data tools.

## 5. Add CI gradually

A useful CI ladder:

1. formatting/static checks if you have them
2. `make test`
3. Python tests
4. install smoke
5. optional csynth on one small kernel if runners have Vitis
6. hardware tests only on dedicated machines

Do not make every pull request run full hardware link unless you have enough machines and time.
