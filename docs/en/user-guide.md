# User guide: command reference with context

Use this page after reading [Concepts](concepts.md). It is a quick reference for commands, but each command also says what it does.

## 1. First checks

```bash
make test
```

Runs CPU-only tests. Use this before Vitis commands.

```bash
make python-env
```

Creates `.venv` for Python tools. Normal `make build` does not do this.

## 2. Build host apps

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

Builds one CPU host executable. It does not synthesize kernels and does not link xclbin.

## 3. HLS kernel commands

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

Runs Vitis HLS synthesis for one kernel.

```bash
make cosim TARGET=u250 KERNEL=saxpy
```

Runs C/RTL cosimulation for one kernel.

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Summarizes synthesis/cosim reports.

## 4. xclbin and hardware

```bash
make xclbin TARGET=u250
```

Links synthesized kernels into an FPGA binary.

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Runs a host app locally against an installed FPGA card.

```bash
make test-xrt-hw TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=192.168.1.10
```

Deploys/runs on a remote embedded board through SSH.

## 5. Data commands

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make compare DATASET=tiny
```

Generate input, compute CPU expected output, and compare hardware output.

## 6. Variables

| Variable | Meaning |
|---|---|
| `TARGET` | board config under `config/<target>/` |
| `KERNEL` | HLS kernel target |
| `HOST_APP` | CPU executable target |
| `DATASET` | data directory name |
| `BOARD_IP` | remote board IP for deploy/run |
| `PETALINUX_SYSROOT` | embedded host build sysroot |
| `PYPI_INDEX` | Python package index; empty disables mirror |

## 7. More detailed pages

- [Build guide](build.md) explains each command in detail.
- [Customization guide](customization.md) shows how to add a kernel.
- [Deployment guide](deploy.md) explains remote board runs.
