# hlsflow

`hlsflow` parses Vitis HLS csynth / cosim / link reports, renders rich console output,
saves HTML + plain-text snapshots, and appends a JSONL record to
`reports/runs.jsonl` for later threshold checks and diffs.

## Usage

```bash
PYTHONPATH=tools python -m hlsflow collect --build-dir build/u250-host --kernel saxpy
PYTHONPATH=tools python -m hlsflow collect --build-dir build/u250-host --kernel all
PYTHONPATH=tools python -m hlsflow collect --build-dir build/u250-host --kernel saxpy --target cosim
PYTHONPATH=tools python -m hlsflow collect --build-dir build/u250-host --kernel saxpy --target link
PYTHONPATH=tools python -m hlsflow check --max-ii 1 --max-lut 240000 --max-dsp 1920
PYTHONPATH=tools python -m hlsflow compare --baseline saxpy_u250_20260515_093000 \
                          --candidate saxpy_u250_20260516_103000
```

## Run record schema

Each line of `reports/runs.jsonl` is a `RunRecord` (see `database.py`):

| Field | Type |
|-------|------|
| `run_id` | `<kernel>_<platform>_<UTC-yyyymmdd_HHMMSS_us>_<suffix>` |
| `kernel`, `platform`, `target` | strings (target ∈ {csynth, cosim, link}) |
| `git_commit`, `build_dir`, `vitis_version`, `status`, `timestamp` | strings |
| `reports` | dict (paths to source XML / HTML / TXT, cosim dirs, or link artifacts) |
| `metrics` | dict (csynth: worst_loop_ii, latency/resource/timing; cosim: RTL status/latency; link: xclbins, compute units, memory connections, clocks, Vitis warnings/errors) |

## Vitis version compatibility

`parse_csynth.py` and `parse_cosim.py` try multiple XPath / filename patterns
before giving up. If you hit a Vitis version where a field is missing, extend
the `_RPT_GLOBS` / `_XML_GLOBS` tuples in `parse_cosim.py` or the XPath lists
in `parse_csynth.py`.
