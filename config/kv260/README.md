# config/kv260/

KV260/K26 embedded target configuration.

| Variable | Value |
| --- | --- |
| `ANVIL_DEVICE_KIND` | `embedded` |
| `ANVIL_VITIS_PART` | `xck26-sfvc784-2lv-c` |
| `ANVIL_PRESET` | `kv260-kernel` |
| `ANVIL_HOST_PRESET` | `kv260-host` |

Notes:

- `make build-host TARGET=kv260` cross-compiles the AArch64 host and requires
  `PETALINUX_SYSROOT`.
- Kernel/xclbin flows require a KV260/K26 Vitis platform. The default preset uses
  `$(XILINX_VITIS)/base_platforms/xilinx_kv260_base_202420_1/...`; override the
  platform path if your install uses a board-specific KV260 platform package.
