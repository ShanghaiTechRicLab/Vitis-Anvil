# Accelerator-card flow

Accelerator cards (U250, U50, U55C, U200, U280, VCK5000) connect over PCIe and use XRT for runtime management. The flow is the same regardless of which card you pick — you just change the `TARGET` variable.

## 1. Install and discover platforms

A working card flow needs three things:

1. Vitis tools (`v++`, `vitis-run`, etc.)
2. XRT headers, libraries, and runtime
3. A platform `.xpfm` file that matches your card and shell version

Find which platform files are available on your system:

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

If the default path in `config/<target>/anvil.mk` is wrong for your setup, override it on the command line:

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

Once you have a stable path, you can also edit `config/u50/anvil.mk` directly to make it permanent.

## 2. Build the host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

Which host apps are available:

| `HOST_APP=` | What it does |
|---|---|
| `run_saxpy` | saxpy demo host |
| `run_vadd` | vadd demo host |
| `run_pipeline_demo` | accelerator-card stream pipeline demo |

`run_pipeline_demo` requires `config/<target>/pipeline_demo.cfg`. The repository provides these for `u250`, `u55c`, `u50`, `u200`, `u280`, and `vck5000`.

## 3. Synthesize and cosimulate kernels

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Use `KERNEL=vadd` for the vadd kernel or `KERNEL=all` for everything configured for this target. The stream pipeline has its own targets:

```bash
make csynth-stream TARGET=u250   # also u55c, u50, u200, u280, vck5000
make cosim-stream TARGET=u250    # also u55c, u50, u200, u280, vck5000
```

## 4. Link and run the xclbin

```bash
make xclbin TARGET=u250
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

For hardware emulation (no physical card):

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## 5. U250 pipeline demo

```bash
make pipeline-demo TARGET=u250
make run-host TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny

# Same commands work for u55c/u50/u200/u280/vck5000 when the matching platform is installed.
```

The pipeline demo builds a separate `pipeline_demo.xclbin`. The Makefile picks it automatically when you set `HOST_APP=run_pipeline_demo`.

## 6. Card health checks

Before debugging your own code, make sure the card and XRT are working:

```bash
xbutil examine
xbutil validate -d 0
xbutil --version
```

## Common problems

| Symptom | Likely cause | Fix |
|---|---|---|
| `.xpfm missing` | Platform package not installed, or path differs | Use `find ... -name '*.xpfm'` and set `ANVIL_PLATFORM=` |
| `Could NOT find XRT` | XRT not installed, or not under `/opt/xilinx/xrt` | Install or source XRT; update `cmake/FindXRT.cmake` hints if needed |
| xclbin load fails | xclbin was linked for a different shell version | Rebuild with the exact platform that matches the installed shell |
| Host app runs but compare fails | Dataset, golden reference, and host output do not agree on format | Check `scripts/gen_dataset.py`, the gold code, and the host output path |
