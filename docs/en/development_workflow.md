# Development workflow

The cardinal rule: use the cheapest check that can catch the bug before running a slower FPGA step. A five-minute CPU test beats a two-hour xclbin rebuild.

## 1. The debugging ladder

Always work from cheapest to most expensive:

```text
gold/unit tests → HLS model (make test) → csynth → cosim → xclbin → swemu → hwemu → hw
```

| Ladder rung | Command | Catches | Does not catch |
|---|---|---|---|
| Gold and CPU unit tests | `make test` | Normal C++/Python mistakes, data-format bugs | Anything FPGA-specific |
| HLS model | `make test` (included) | Pack/stream/dataflow/tail-element bugs before Vitis | Timing, resources, RTL correctness, XRT |
| csynth | `make csynth` + `make analyze` | HLS-incompatible C++, II violations, timing, resources | RTL-vs-C++ mismatch, host/XRT bugs |
| cosim | `make cosim` | RTL behavior mismatch, interface protocol bugs | Host/XRT bugs, platform-specific issues |
| xclbin | `make xclbin` + `make analyze-link` | Connectivity errors, memory bank mismatches | Runtime host bugs |
| swemu | `make swemu` | Host app API, BO setup, argument passing | RTL correctness (sw_emu uses the C model) |
| hwemu | `make hwemu` | RTL correctness, interface timing | Physical timing, card-specific bugs |
| hw | `make hw` | All of the above + real hardware issues | — |

Do not jump from editing a kernel directly to hardware run. Waiting hours only to discover a simple argument-order bug is avoidable.

---

## 2. When changing normal C++ or Python code

```bash
make test
```

This is the full CPU-only test suite: gold, HLS model, utilities, and tests. It is the first FPGA-shaped check. It should complete in a few minutes.

If Python tools changed:

```bash
make python-env
make test
```

---

## 3. When changing a kernel

Run the full synthesis and cosim ladder:

```bash
make test                                      # catch CPU-side bugs first
make csynth TARGET=u250 KERNEL=<kernel>
make analyze TARGET=u250 KERNEL=<kernel>       # check II, timing, resources
make cosim  TARGET=u250 KERNEL=<kernel>
make analyze-cosim TARGET=u250 KERNEL=<kernel>
```

**Kernel file roles (using saxpy as example):**

| File | Role | Change triggers |
|---|---|---|
| `src/gold/cpp/saxpy_gold.cpp` | CPU truth | csynth/cosim should not change |
| `src/kernels/include/kernels/saxpy_core.hpp` | Shared core: Load/Compute/Store + op | Both HLS model and Vitis top |
| `src/kernels/saxpy_kernel.cpp` | Vitis top: ABI, pragmas, dataflow | csynth + cosim |
| `src/hls_model/saxpy_hls_model.cpp` | CPU model mirroring kernel structure | CPU tests only |
| `src/host/run_saxpy.cpp` | XRT host | Host build + hardware run |

**Interpreting failures:**

| Failure | Meaning | Fix |
|---|---|---|
| csynth compile error | Kernel uses HLS-incompatible C++ or missing include | Fix kernel code or CMake include path |
| II > 1 | Pipeline has a loop-carried dependency or memory port conflict | Restructure the loop, add `#pragma HLS array_partition`, or reduce II target |
| Negative timing slack | Logic path too long for the requested clock | Lower clock frequency, retime with pipeline pragma, or simplify logic |
| cosim fails | RTL behaves differently from C++ kernel | Debug kernel logic or testbench; do not move on to xclbin |

---

## 4. When changing the host app

```bash
make build TARGET=u250 HOST_APP=<app>
```

This only compiles the C++ host. It should not synthesize kernels.

If the host app compiles but runtime fails, diagnose in this order:

1. **XRT not sourced** — run `. /opt/xilinx/xrt/setup.sh` and `xbutil examine`
2. **Wrong xclbin path** — check that `make xclbin` completed successfully
3. **Compute unit name** — must match `link.cfg` `nk=` instance name exactly
4. **BO group index** — must match the order of pointer arguments in the kernel C++ signature
5. **Dataset path** — is `data/<dataset>/` populated from `make gen`?

---

## 5. When changing link.cfg

```bash
make xclbin TARGET=u250
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

`link.cfg` changes are not caught by C++ unit tests or cosim. They are caught by the Vitis linker (wrong bank names produce link errors) and by host runtime (wrong compute unit name produces "kernel not found").

Always run `analyze-link` after changing `link.cfg` before touching the host app.

---

## 6. When adding a new board target

Start with configure and synthesis only. Do not jump to hardware:

```bash
make csynth TARGET=<target> KERNEL=saxpy
make analyze TARGET=<target> KERNEL=saxpy
```

If synthesis passes, proceed:

```bash
make xclbin      TARGET=<target>
make analyze-link TARGET=<target> HOST_APP=run_saxpy
make build       TARGET=<target> HOST_APP=run_saxpy
make swemu       TARGET=<target> HOST_APP=run_saxpy DATASET=tiny
make hwemu       TARGET=<target> HOST_APP=run_saxpy DATASET=tiny
make hw          TARGET=<target> HOST_APP=run_saxpy DATASET=tiny  # if card is present
```

`analyze-link` is the checkpoint before host. It confirms the xclbin is valid and ports are bound as expected.

For embedded targets, set `PETALINUX_SYSROOT` before the host build step.

---

## 7. Emulation workflow details

For software emulation, the emconfig file must exist:

```bash
make emconfig TARGET=u250 MODE=sw_emu  # generate once per target
make swemu    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

For hardware emulation:

```bash
make emconfig TARGET=u250 MODE=hw_emu  # same file as sw_emu, skips if already built
make hwemu    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Use tiny datasets for hw_emu. RTL simulation of large data transfers is very slow.

When debugging emulation failures:
- If `swemu` passes but `hwemu` fails: RTL has a bug the C model did not catch. Rerun cosim.
- If both `swemu` and `hwemu` fail: host app has a bug independent of RTL. Debug the BO setup.
- If `hwemu` passes but `hw` fails: likely a physical timing or board-specific issue.

---

## 8. Commit strategy

Good commits are small and layer-based:

1. Gold/reference change + tests
2. Kernel ABI/header change
3. Kernel implementation + cosim testbench
4. CMake/Make registration
5. Host app change
6. Board config / link.cfg change
7. Documentation update

Do not mix "new kernel implementation" and "new board config" in the same commit unless they are inseparable.

---

## 9. What to record after a significant change

For any kernel or board change, record:

- Commands run and targets tested
- Whether csynth II and timing are acceptable
- Whether cosim passed
- Whether hardware link was run
- Whether real hardware was tested, or which emulation mode was used
- Any known untested paths (older XRT fallback, embedded sysroot path, hw_emu skipped due to slowness)

This record is useful when someone else (or your future self) picks up the work.
