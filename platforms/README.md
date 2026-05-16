# platforms/

Board-specific metadata for supported and planned embedded targets.

| Directory | Board | Status |
|-----------|-------|--------|
| `zcu102/` | Xilinx ZCU102 (xczu9eg) | First-class — Phase 4 |
| `kv260/`  | Xilinx Kria KV260 (xck26) | Experimental config — requires external KV260 platform |
| `zcu104/` | Xilinx ZCU104 (xczu7ev) | Config exists; no `platforms/` entry yet |

## File layout per board

| File | Purpose |
|------|---------|
| `board.toml` | Canonical metadata: part, platform name, HP ports, deploy defaults |
| `xrt.ini` | XRT runtime + emulation config template (first-class boards; stubs may omit until implemented) |
| `README.md` | Lab setup notes: JTAG, network, sysroot path |
