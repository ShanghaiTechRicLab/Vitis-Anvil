# Vitis-Anvil

![Vitis-Anvil banner](docs/assets/vitis-anvil-banner.png)

A CMake project template for forging Vitis/XRT FPGA accelerators. You bring the kernel and the host code; Anvil — the iron block where hardware is forged into shape — gives you the build system, the test framework, and the deployment tooling.

If you are new here, start with the English or Chinese documentation:

- English: [README.en.md](README.en.md) · [docs/en/](docs/en/index.md) · [concepts](docs/en/concepts.md)
- 中文: [README.zh.md](README.zh.md) · [docs/zh/](docs/zh/index.md) · [基本概念](docs/zh/concepts.md)
- hlslib adaptation: [English](docs/en/hlslib_adaptation.md) · [中文](docs/zh/hlslib_adaptation.md)
- Recommended flow: `gold -> hls_model -> csynth -> cosim -> xclbin -> swemu/hwemu/qemu/hw`.
