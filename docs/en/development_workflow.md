# Typical development workflow

The main idea: keep fast CPU iteration separate from slow FPGA work. You do not want to wait for HLS synthesis just because you fixed a typo in a host file, and you do not want to debug host logic through Vitis logs.

## Day-to-day loop

```bash
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

Run this after normal C++ or Python changes. It does not create a Python environment unless you call `make python-env` explicitly, and it does not synthesize kernels. It just compiles the host app and runs CPU-side tests.

## Kernel loop

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

Use this when you are changing HLS code or pragmas. Look at:

- II (initiation interval)
- Latency
- Timing slack
- Resource headroom (LUT, DSP, BRAM)
- Interface summary

## Cosim loop

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosimulation runs the RTL that Vitis generated from your kernel against the kernel testbench. This catches ABI mismatches and interface bugs before you build an xclbin. It does not run the XRT host program.

## XRT host loop

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make xclbin TARGET=u250
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Run this after changing the kernel ABI or the host buffer logic.

## Embedded board loop

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## Before committing

```bash
make test
make analyze-flow TARGET=<target> KERNEL=<kernel>
make analyze-cosim TARGET=<target> KERNEL=<kernel>
```

If your change touches hardware, also run `make run-host` or the board deployment path.

## Debugging order

1. `make test` fails: fix the CPU-side logic, library issues, or golden reference first.
2. `make csynth` fails: check HLS compiler logs and C++14/HLS restrictions.
3. `make cosim` fails: check the kernel testbench and ABI assumptions.
4. `make xclbin` fails: check `link.cfg`, platform, memory banks, clock constraints.
5. Host run fails: check XRT device selection, xclbin path, CU name, buffer group IDs.
6. Compare fails: check the dataset, golden reference, and host output path — they need to agree on the data format.
