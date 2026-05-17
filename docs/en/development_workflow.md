# Development workflow

This page gives the day-to-day order for changing code. The main rule is: use the cheapest check that can catch the bug before using a slower FPGA step.

## 1. The debugging ladder

Use this order:

```text
gold -> hls_model -> csynth -> cosim -> xclbin -> host
```

1. **Gold and CPU unit tests** — catches normal C++/Python mistakes.
2. **HLS model (`make test-hls-model`)** — runs the pre-Vitis CPU suite: gold + FPGA-shaped HLS model + HLS helper tests.
3. **HLS synthesis (`csynth`)** — catches HLS-incompatible C++ and reports estimated hardware.
4. **HLS cosim (`cosim`)** — catches RTL behavior mismatch.
5. **xclbin link** — catches platform/connectivity/memory-bank problems.
6. **host build** — catches XRT/API/compile problems.
7. **hardware run** — catches runtime, BO group, device, and deployment problems.
8. **compare/analyze** — checks correctness and performance trends.

Do not jump from editing a kernel directly to hardware run. You will wait longer and get less precise errors.

The HLS model is the first FPGA-shaped check: it packs scalar data, uses the same project-owned kernel core helpers as the Vitis top, and catches pack/stream/dataflow/tail bugs before Vitis. It is still a CPU simulation, so it does not prove timing, resource use, link connectivity, XRT buffer groups, or board deployment.

## 2. When changing normal C++ or Python code

Run:

```bash
make test
```

`make test` and `make test-hls-model` currently run the same pre-Vitis CPU suite. Use the explicit name when you want to document that the HLS model is the first FPGA-shaped check after the plain gold/unit tests:

```bash
make test-hls-model
```

If Python tools are involved:

```bash
make python-env
make test
```

This should be fast and should not need Vitis or XRT.

## 3. When changing a kernel

Run:

```bash
make test-hls-model
make csynth TARGET=u250 KERNEL=<kernel>
make analyze-flow TARGET=u250 KERNEL=<kernel>
make cosim TARGET=u250 KERNEL=<kernel>
make analyze-cosim TARGET=u250 KERNEL=<kernel>
```

For `saxpy`, the files have these roles:

```text
src/gold/include/gold/saxpy_gold.hpp              # plain CPU truth
src/kernels/include/kernels/saxpy_core.hpp        # shared kernel core and stage helpers
src/kernels/saxpy_kernel.cpp                      # Vitis top: ABI, pragmas, explicit dataflow
src/hls_model/include/hls_model/saxpy_hls_model.hpp
src/hls_model/saxpy_hls_model.cpp                 # scalar adapter around the shared core
src/host/run_saxpy.cpp                            # XRT host app
```

Keep that split for new m_axi-style packed kernels: gold first, then HLS model, then csynth/cosim/xclbin/host. Stream/k2k kernels are supported by the existing Vitis flow, but first-class stream HLS models are not yet the default pattern.

Interpretation:

- csynth compile error: kernel code or include path issue
- csynth II/timing bad: HLS structure issue
- cosim fail: algorithm/RTL mismatch or testbench issue
- analyze output weird: parser or report location issue

## 4. When changing host code

Run:

```bash
make build TARGET=u250 HOST_APP=<app>
```

This only builds the host app. It should not synthesize kernels.

If the host app compiles but runtime fails, check:

- XRT setup
- xclbin path
- compute-unit name
- BO group index
- dataset path

## 5. When changing `link.cfg`

Run:

```bash
make xclbin TARGET=u250
```

`link.cfg` changes are not tested by C++ unit tests. They are tested by the Vitis linker and then by host runtime.

## 6. When adding a board

Start with configure-only and synthesis before hardware:

```bash
make csynth TARGET=<target> KERNEL=saxpy
make analyze-flow TARGET=<target> KERNEL=saxpy
```

Only after that try:

```bash
make xclbin TARGET=<target>
make analyze-link TARGET=<target> HOST_APP=run_saxpy
make build TARGET=<target> HOST_APP=run_saxpy
```

`analyze-link` is the checkpoint between xclbin and host. It confirms that the xclbin exists, which compute units were linked, and how kernel ports were bound to DDR/HBM banks.

For embedded boards, build host with `PETALINUX_SYSROOT` set.

## 7. Commit strategy

Good commits are small and layer-based:

1. gold/reference change + tests
2. kernel ABI/header change
3. kernel implementation + cosim
4. CMake/Make registration
5. host app change
6. board config/link.cfg change
7. docs update

Do not mix “new kernel implementation” and “new board config” in the same commit unless they are inseparable.

## 8. What to record in a final note

For any significant change, record:

- commands run
- targets tested
- whether Vitis hardware link was run
- whether real hardware was run
- known untested branches, such as older XRT fallback or embedded sysroot path
