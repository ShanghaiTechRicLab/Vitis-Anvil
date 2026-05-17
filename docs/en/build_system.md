# Build system model

Vitis-Anvil treats the FPGA flow as a staged artifact graph, not as a pile of shell scripts.

The core rule is:

```text
user command -> real artifact stamp -> stage-specific action key -> manifest
```

A Make target such as `csynth`, `xclbin`, or `hwemu` may be phony, but it must depend on real files. Long-running Vitis work is owned by CMake/Ninja rules that produce a stamp and declare their byproducts.

## Layers

### 1. User command layer

Examples:

```bash
make csynth TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250 HOST_APP=run_saxpy
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny
make analyze TARGET=u250 KERNEL=saxpy
```

These commands select intent. They should not hide unconditional Vitis runs.

### 2. Artifact action layer

Every heavy action is modeled like this:

```cmake
add_custom_command(
  OUTPUT      <stage>.stamp
  BYPRODUCTS real.outputs action.json action.sha256 env.json env.sha256 manifest.json
  DEPENDS     declared.inputs
  DEPFILE     deps.d
  COMMAND     python -m hlsflow.action_runner ... -- actual-tool ...
)
```

The stamp is touched only after the command succeeds. If Vitis fails halfway through, the stamp is not updated.
Today `action.sha256` is written as explainable metadata beside the stamp; Ninja
does not use a file produced by the same command to decide whether to rerun that
command. Inputs that must invalidate the local build must still appear in
`DEPENDS`, `DEPFILE`, or the custom command line. The metadata is the hook for a
future precomputed action-cache/CAS layer.

### 3. Cache metadata layer

Each action writes:

| File | Meaning |
|---|---|
| `action.json` | canonical stage inputs, command, config, and output names |
| `action.sha256` | policy hash for diagnostics/manifests; future CAS key |
| `env.json` | diagnostic tool/environment metadata |
| `env.sha256` | soft environment hash; strict mode may make it invalidating |
| `manifest.json` | produced files, sizes, and action hash |

Ninja still handles local incremental builds. The metadata makes each action explainable and leaves room for future content-addressed or remote caching.

## Stage-specific keys

Do not put `MODE` into every key. Each stage decides what affects its output.

| Stage | Mode in key? | Notes |
|---|---:|---|
| `data` | no | dataset params, seed, dtype, and generator decide the result |
| `gold` | no | data digest and gold logic decide the result |
| `hls-model` | no | CPU-only; no Vitis, XRT, platform, or board target |
| `host` | usually no | accelerator host binary is shared across `sw_emu`, `hw_emu`, and `hw` when ABI inputs match |
| `csynth` | only if real compile config differs | `.xo` should not be triplicated by mode by default |
| `cosim` | only if simulator config differs | testbench/data changes rerun cosim, not csynth |
| `xclbin` | yes | `sw_emu`, `hw_emu`, and `hw` xclbins differ |
| `emconfig` | no between sw/hw emu | one target-level `emconfig.json` is shared |
| `run` | yes | output is separated by target/mode/host/dataset/run key |
| `qemu` | embedded-only | prepares AArch64 host + embedded emulation xclbin + emconfig; actual QEMU launch is platform-image specific |
| `analyze` | no | strict read-only by default; `BUILD=1` opts into building missing reports |
| `deploy` | yes | bundle creation can be cached; SSH copy is a side effect |

## Important paths

```text
build/<preset>/src/kernels/<kernel>_hls/.csynth.stamp
build/<preset>/src/kernels/<kernel>_hls/<kernel>.xo
build/<preset>/src/kernels/<kernel>_hls/.csynth.d
build/<preset>/src/kernels/<kernel>_hls/csynth.manifest.json

build/<preset>/src/kernels/<xclbin>_xclbin/.link.stamp
build/<preset>/src/kernels/<xclbin>_xclbin/<xclbin>.xclbin
build/<preset>/src/kernels/<xclbin>_xclbin/link.manifest.json

build/<target>/emconfig/emconfig.json
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/out.bin
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/run.json
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/stdout.log
```

`data/` is input-only. Runtime outputs do not go under `data/`.

## Header dependencies

HLS C++ dependencies are tracked with depfiles. The build does not glob whole include directories as the final dependency model.

The dep scanner uses compiler-style dependency generation:

```text
-MMD -MP -MF .csynth.d -MT .csynth.stamp
```

Generated headers must exist before scanning. Missing headers should fail instead of being hidden.

## Analyze behavior

`make analyze` reads existing reports. It should not unexpectedly start an hour-scale Vitis build.

Use this to parse what exists:

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

Use this when you explicitly want missing reports built first:

```bash
make analyze TARGET=u250 KERNEL=saxpy BUILD=1
```

## Why this matters

FPGA builds are expensive. The build graph must answer four questions for every edge:

1. Why does this action need to run?
2. Why is it safe not to run?
3. Where are the outputs?
4. If the action fails, did the stamp remain clean?

If a rule cannot answer those questions, it is not build-systemized yet.

## External models used by this design

- CMake `add_custom_command(OUTPUT ... BYPRODUCTS ... DEPFILE ...)` is the
  primitive used to tell Ninja which files a rule owns.
- Ninja depfiles are the right model for C/C++ header discovery; hand-written
  directory globs are intentionally avoided.
- Bazel's action-cache/CAS model informs the `action.json` + `manifest.json`
  metadata, but the current implementation still relies on local Ninja
  timestamps for invalidation.
- AMD `emconfigutil` supports one `emconfig.json` for software and hardware
  emulation, so Anvil stores it once under `build/<target>/emconfig/`.
- Similar public CMake/Vitis-HLS projects exist, but most stop at wrapping
  Vitis commands. Anvil's extra requirement is explainable stage-specific
  invalidation with stamps, byproducts, depfiles, and manifests.
