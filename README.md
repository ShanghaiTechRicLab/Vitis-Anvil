# Vitis-Anvil

A Vitis + XRT HLS project **template** with batteries included.

Clone this repo, replace the saxpy demo with your kernel, and get working
HLS synthesis → co-simulation → FPGA execution with gold comparison — in minutes.

## Quick start

```bash
rtk git clone <repo>
rtk pip install -e .
make test                   # fast, CPU-only — no Vitis or FPGA needed
```

## What's inside

| Role | What you get |
|------|-------------|
| **Template** | Working saxpy demo; replace the listed demo touch-points to use your own kernel |
| **Library** | `anvil::*` C++ (11 sub-namespaces) + `anvil.*` Python (9 modules) |
| **Scaffold** | CMake modules, `TARGET=<platform>` Makefile, per-device config |

## Target platforms

| `TARGET=` | Device | Status |
|-----------|--------|--------|
| `u250` | Alveo U250 | First-class |
| `zcu104` | ZCU104 (AArch64) | First-class |
| `zcu102` | ZCU102 (AArch64) | First-class |

## Key commands

```bash
make build TARGET=u250       # configure + build (needs Vitis + U250 .xpfm + XRT)
make csynth                  # v++ HLS synthesis → .xo
make xclbin-hwemu            # link .xclbin for hw_emu
make xrt-emu DATASET=tiny    # run on hw_emu
make compare DATASET=tiny    # compare outputs already produced for the dataset
make gold ANVIL_LANG=python  # run Python gold reference
```

## Replacing the saxpy demo

Edit these demo touch-points:

1. `src/kernels/saxpy_kernel.cpp` — your HLS kernel
2. `src/host/run_saxpy.cpp` — your XRT host
3. `src/hls_model/saxpy_hls_model.cpp` — CPU parity model (optional)
4. `src/gold/cpp/saxpy_gold.cpp` — C++ reference
5. `src/gold/python/saxpy_gold.py` — Python reference
6. `config/u250/link.cfg` / `config/zcu104/link.cfg` / `config/zcu102/link.cfg` — kernel port→memory/interface mapping
7. `scripts/gen_dataset.py` — input data format
8. `scripts/compare.py` — output comparison format

See `docs/user-guide.md` for details.

## C++ library (`anvil::*`)

`#include <anvil/log/anvil_log.hpp>` (and 10 more sub-modules).
Link: `target_link_libraries(... PRIVATE anvil::log anvil::compare anvil::runtime)`

## Python library (`anvil.*`)

```python
import anvil.log
import anvil.compare
```

## License

See `LICENSE`. Vendored third-party code retains its upstream licenses under
`third_party/<lib>/LICENSE*` and is summarized in `third_party/THIRD_PARTY_NOTICES.md`.
