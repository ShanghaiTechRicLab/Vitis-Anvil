# Accelerator-card flow

This flow targets PCIe/XRT accelerator cards such as U250, U50, U55C, U200, U280, and VCK5000.

## 1. Install and discover platforms

A working card flow needs three things:

1. Vitis tools (`v++`, `vitis-run`).
2. XRT headers/libraries/runtime.
3. The matching platform `.xpfm` for the card and shell installed on your machine.

Discover platform files:

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

If the default path in `config/<target>/anvil.mk` is wrong, override it:

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

You can also edit `config/u50/anvil.mk` once the path is stable for your lab.

## 2. Build host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

Host app choices:

| `HOST_APP=` | Purpose |
|---|---|
| `run_saxpy` | saxpy demo host |
| `run_vadd` | vadd demo host |
| `run_pipeline_demo` | accelerator-card stream pipeline demo |

`HOST_APP=run_pipeline_demo` requires `config/<target>/pipeline_demo.cfg`. The repository provides configs for `u250`, `u55c`, `u50`, `u200`, `u280`, and `vck5000`.

## 3. Synthesize and cosim kernels

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Use `KERNEL=vadd` for vadd, or `KERNEL=all` for the target's configured default kernel set. The stream pipeline uses:

```bash
make csynth-stream TARGET=u250   # also u55c/u50/u200/u280/vck5000
make cosim-stream TARGET=u250    # also u55c/u50/u200/u280/vck5000
```

## 4. Link and run xclbin

```bash
make xclbin TARGET=u250
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

For hardware emulation:

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## 5. U250 pipeline demo

```bash
make pipeline-demo TARGET=u250
make run-host TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny

# Same target shape is available for u55c/u50/u200/u280/vck5000 when the matching platform is installed.
```

The pipeline demo builds a separate `pipeline_demo.xclbin`; the Makefile selects it automatically when `HOST_APP=run_pipeline_demo`.

## 6. Card health checks

Before debugging your code, verify the card and XRT:

```bash
xbutil examine
xbutil validate -d 0
xbutil --version
```

Common problems:

| Symptom | Likely cause | Fix |
|---|---|---|
| `.xpfm missing` | platform package not installed or path differs | use `find ... -name '*.xpfm'` and set `ANVIL_PLATFORM=` |
| `Could NOT find XRT` | XRT headers/libraries not installed or not under `/opt/xilinx/xrt` | install/source XRT or update `cmake/FindXRT.cmake` hints |
| xclbin load fails | xclbin linked for a different platform shell | rebuild with the exact platform matching the installed shell |
| host app runs but compare fails | dataset/gold/host output contract mismatch | inspect `scripts/gen_dataset.py`, gold, and host output path |
