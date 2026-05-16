# src/kernels/

HLS kernels for Vitis-Anvil.

| Kernel | Top | Source | Cosim TB |
|--------|-----|--------|----------|
| saxpy  | saxpy | saxpy_kernel.cpp | tests/kernels/saxpy_cosim_tb.cpp |
| vadd   | vadd  | vadd_kernel.cpp  | tests/kernels/vadd_cosim_tb.cpp |

On u250/u55c, both kernels link into a single `saxpy.xclbin` via
`add_anvil_xclbin(KERNEL_TARGETS saxpy_xo vadd_xo ...)`.
Embedded targets (zcu102, zcu104) keep saxpy-only xclbin; `vadd_xo` is still
a registered buildable target but not included in the xclbin link. See
`config/<board>/link.cfg` for per-kernel `nk=`/`sp=` mappings.
