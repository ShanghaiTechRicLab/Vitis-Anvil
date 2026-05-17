# Vitis-Anvil documentation

Vitis-Anvil is a CMake project template for building Vitis/XRT FPGA accelerators. The documentation assumes only basic FPGA/HLS awareness. It explains each step before asking you to run commands.

Recommended reading order:

1. [Concepts](concepts.md) — what kernel, host app, xclbin, XRT, csynth, cosim, and TARGET/KERNEL/HOST_APP mean.
2. [Get started](get_started.md) — run the project from CPU-only tests to hardware-ready artifacts.
3. [Project structure](project_structure.md) — where each layer lives and why files are separated.
4. [Build guide](build.md) — what every common Make target does, what it produces, and when to use it.
5. [Build system model](build_system.md) — stage-specific action graph, stamps, depfiles, action keys, and runtime output layout.
6. [Accelerator-card flow](accelerator_flow.md) — the path for U250/U50/U55C/U200/U280/VCK5000-style PCIe cards.
7. [Embedded flow](embedded_flow.md) — the path for ZCU102/ZCU104/ZCU106/KV260-style boards.
8. [Deployment guide](deploy.md) — how files move to a real board/card and how to debug that step.
9. [Customization guide](customization.md) — add your own kernel, host app, dataset, board, and reports.
10. [Adapt to your project](adapt_to_your_project.md) — turn the template into a product repository.
11. [Development workflow](development_workflow.md) — day-to-day loop and debugging order.
12. [hlslib adaptation](hlslib_adaptation.md) — reusable pack/stream/dataflow helpers and framework/user boundary.
13. [HLS architecture primitives](hls_primitives.md) — fixed-point, buffers, burst helpers, reductions, small sorters, and double-buffered skeletons.
14. [User guide](user-guide.md) — command reference and quick lookup.

If you are lost, go back to [Concepts](concepts.md). Most confusion comes from mixing up kernel-level steps (`csynth`, `cosim`, `xclbin`) with host-level steps (`build-host`, `hw`, deploy).
