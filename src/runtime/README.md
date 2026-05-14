# Legacy runtime directory

The XRT runtime wrappers now live under `include/anvil/runtime/` and
`src/anvil/runtime/`. This directory is retained only so older scaffold paths
remain understandable; root CMake no longer adds `src/runtime`.
