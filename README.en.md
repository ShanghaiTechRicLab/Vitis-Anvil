# Vitis-Anvil

![Vitis-Anvil banner](docs/assets/vitis-anvil-banner.png)

**A CMake project template for forging Vitis/XRT FPGA accelerators.**

Anvil is the iron block you shape metal on. Vitis-Anvil is that same idea for FPGA work: a project skeleton that covers the common pieces — CMake presets, Vitis HLS kernels, XRT host programs, CPU golden references, datasets, HLS/cosim analysis, board deployment helpers, per-device configuration — so you can focus on the actual accelerator logic.

The name is short because it is meant to be used on the command line. Future wrappers like `anvil init`, `anvil build`, or `anvil csynth` map naturally onto the Makefile and CMake targets documented here.

## Start here

- [Documentation index](docs/en/index.md) — read in order if you are new
- [Concepts](docs/en/concepts.md) — explains kernel, host app, xclbin, XRT, csynth, cosim
- [Get started: run the full flow](docs/en/get_started.md) — from clone to hardware
- [Accelerator-card flow](docs/en/accelerator_flow.md) — U250/U50/U55C/U200/U280/VCK5000
- [Embedded flow](docs/en/embedded_flow.md) — ZCU102/ZCU104/ZCU106/KV260
- [Customize kernels, host apps, devices, datasets, and reports](docs/en/customization.md)
- [Adapt Vitis-Anvil to your own project](docs/en/adapt_to_your_project.md)
- [Typical development workflow](docs/en/development_workflow.md)
- [hlslib adaptation pattern](docs/en/hlslib_adaptation.md)

## What is included

| Area | Included pieces |
|---|---|
| Build system | CMake presets, `config/<target>/anvil.mk`, top-level Make targets |
| Kernels | `saxpy`, `vadd`, and accelerator-card stream pipeline demos |
| Host apps | `run_saxpy`, `run_vadd`, `run_pipeline_demo` |
| Verification | CPU tests, golden output generation, result comparison, HLS synthesis, HLS cosimulation, XRT hardware run paths |
| Analysis | `hlsflow` console reports, HTML/TXT exports, JSONL run database |
| Targets | Alveo and embedded Zynq/ZynqMP presets, with platform paths you can override |

## Quick command map

```bash
make test                                      # CPU-only tests (no Vitis, no XRT, no platform)
make test-hls-model                            # explicit pre-Vitis suite: gold + HLS model + helpers
make build TARGET=u250 HOST_APP=run_saxpy     # build one host app
make csynth TARGET=u250 KERNEL=saxpy          # HLS synthesis for one kernel
make cosim TARGET=u250 KERNEL=saxpy           # HLS C/RTL cosimulation
make analyze-flow TARGET=u250 KERNEL=saxpy    # inspect synthesis reports
make analyze-cosim TARGET=u250 KERNEL=saxpy   # inspect cosimulation reports
make analyze-link TARGET=u250 HOST_APP=run_saxpy # inspect link/xclbin reports
make xclbin TARGET=u250                       # link the hardware xclbin (slow)
make run-host TARGET=u250 HOST_APP=run_saxpy  # run on an installed accelerator card
```

Recommended correctness ladder: `gold -> hls_model -> csynth -> cosim -> xclbin -> host`.

For embedded boards you need `PETALINUX_SYSROOT`:

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

The full deployment flow is in [docs/en/embedded_flow.md](docs/en/embedded_flow.md).

## License

See [LICENSE](LICENSE). Vendored third-party code keeps its own license under `third_party/`; a summary is in `third_party/THIRD_PARTY_NOTICES.md`.
