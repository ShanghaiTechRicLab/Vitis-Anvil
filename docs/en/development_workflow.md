# Typical development workflow

The core rule: keep fast CPU iteration separate from slow FPGA work.

## Day-to-day loop

```bash
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

Run this after normal C++/Python changes. It should not create a Python environment unless you explicitly call `make python-env`, and it should not synthesize kernels.

## Kernel loop

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

Use this when changing HLS code or pragmas. Check:

- II
- latency
- timing slack
- resource headroom
- interface summary

## Cosim loop

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosim validates the RTL generated from the kernel against the kernel testbench. It does not run the XRT host app.

## XRT host loop

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make xclbin TARGET=u250
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Use this after kernel ABI or host buffer changes.

## Embedded board loop

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## Before committing

Suggested checks:

```bash
make test
make analyze-flow TARGET=<target> KERNEL=<kernel>
make analyze-cosim TARGET=<target> KERNEL=<kernel>
```

For hardware-facing changes, also run the relevant `run-host` or board deployment path.

## Debugging order

1. `make test` fails: fix CPU/library/gold logic first.
2. `make csynth` fails: inspect HLS compile logs and C++14/HLS restrictions.
3. `make cosim` fails: inspect the kernel testbench and ABI assumptions.
4. `make xclbin` fails: inspect `link.cfg`, platform, memory banks, and clock constraints.
5. host run fails: inspect XRT device selection, xclbin path, CU name, buffer group IDs.
6. compare fails: inspect dataset/gold/host output contract.
