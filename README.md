# Vitis-Anvil

**A CMake project template for forging Vitis/XRT FPGA accelerators.**

> 中文读者：见 [README.zh.md](README.zh.md) 和 [docs/zh/](docs/zh/index.md)。

Anvil is the iron block where hardware is forged into shape. Vitis-Anvil gives you a small, reproducible FPGA accelerator project skeleton: CMake presets, Vitis HLS kernels, XRT host programs, CPU gold references, datasets, HLS/cosim analysis, board deployment helpers, and per-device configuration.

This repository is intentionally short-named and CLI-friendly: `anvil init`, `anvil build`, and similar wrappers can be layered on top of the documented Make/CMake flow without fighting the project model.

## Start here

- [Documentation index](docs/en/index.md)
- [Get started: run the full flow](docs/en/get_started.md)
- [Accelerator-card flow: U250/U50/U55C/U200/U280/VCK5000](docs/en/accelerator_flow.md)
- [Embedded flow: ZCU102/ZCU104/ZCU106/KV260](docs/en/embedded_flow.md)
- [Customize kernels, host apps, devices, datasets, and reports](docs/en/customization.md)
- [Adapt Vitis-Anvil to your own project](docs/en/adapt_to_your_project.md)
- [Typical development workflow](docs/en/development_workflow.md)

## What is included

| Area | Included pieces |
|---|---|
| Build system | CMake presets, `config/<target>/anvil.mk`, top-level Make targets |
| Kernels | `saxpy`, `vadd`, and accelerator-card stream pipeline demos |
| Host apps | `run_saxpy`, `run_vadd`, `run_pipeline_demo` |
| Verification | CPU tests, gold generation, comparison, HLS csynth, HLS cosim, XRT run paths |
| Analysis | `hlsflow` rich console reports, HTML/TXT exports, JSONL run database |
| Targets | Alveo and embedded Zynq/ZynqMP-style presets with overrideable platform paths |

## Quick command map

```bash
make test                                      # CPU-only fast tests; no Vitis/XRT/platform needed
make build TARGET=u250 HOST_APP=run_saxpy     # build selected host app
make csynth TARGET=u250 KERNEL=saxpy          # HLS synthesis
make cosim TARGET=u250 KERNEL=saxpy           # HLS C/RTL cosim
make analyze-flow TARGET=u250 KERNEL=saxpy    # csynth report analysis
make analyze-cosim TARGET=u250 KERNEL=saxpy   # cosim report analysis
make xclbin TARGET=u250                       # hardware xclbin link; long-running
make run-host TARGET=u250 HOST_APP=run_saxpy  # run on installed accelerator card
```

For embedded boards, use `PETALINUX_SYSROOT=... make build-host TARGET=zcu102` and the deployment flow in [docs/en/embedded_flow.md](docs/en/embedded_flow.md).

## License

See [LICENSE](LICENSE). Vendored third-party code keeps its upstream licenses under `third_party/` and is summarized in `third_party/THIRD_PARTY_NOTICES.md`.
