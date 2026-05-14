# hlslib (vendored)

- **Upstream:** https://github.com/definelicht/hlslib
- **Version:** v1.4.6
- **Commit:** `f45eb33966999ce9c84a9964870d828b3b569d73`
- **Vendored on:** 2026-05-14
- **License:** see `LICENSE`

## Scope

Only the `include/` subtree is vendored. The upstream `src/`, `tests/`,
`cmake/`, `python/`, `examples/` are **not** included because Phase 0+1
only uses the header-only Stream / DataPack / Simulation modules.

If you need `add_vitis_kernel` / `add_vitis_program` CMake helpers from
upstream `cmake/`, Phase 2 will re-vendor that fragment separately.

## Update procedure

1. `git clone --branch <tag>` upstream
2. Replace `include/` here
3. Update this file's version/commit/date
4. Run a full Level 1 regression (gold build + HLS model build + tests)
   before committing.
