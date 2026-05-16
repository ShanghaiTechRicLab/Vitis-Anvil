"""Static platform metadata used to enrich HLS reports.

These values are fallback/context data. Vitis csynth XML remains the source of
truth when it contains AvailableResources.
"""
from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class PlatformInfo:
    name: str
    part: str
    family: str
    resources: dict[str, int] = field(default_factory=dict)
    memory: str = "unknown"
    default_clock_mhz: int | None = None
    notes: str = ""


_PLATFORMS: dict[str, PlatformInfo] = {
    "zcu102": PlatformInfo(
        name="zcu102",
        part="xczu9eg-ffvb1156-2-e",
        family="zynquplus",
        resources={"LUT": 274080, "FF": 548160, "DSP": 2520, "BRAM_18K": 1824, "URAM": 0},
        memory="PS DDR via HP/HPC ports; Vitis base platform exposes DDR banks",
        default_clock_mhz=200,
        notes="Embedded Zynq UltraScale+ MPSoC; host is AArch64 cross-compiled.",
    ),
    "zcu104": PlatformInfo(
        name="zcu104",
        part="xczu7ev-ffvc1156-2-e",
        family="zynquplus",
        resources={"LUT": 230400, "FF": 460800, "DSP": 1728, "BRAM_18K": 624, "URAM": 0},
        memory="PS DDR via HP/HPC ports; Vitis base platform exposes DDR banks",
        default_clock_mhz=200,
        notes="Embedded Zynq UltraScale+ MPSoC; smaller PL than ZCU102.",
    ),
    "u250": PlatformInfo(
        name="u250",
        part="xcu250-figd2104-2L-e",
        family="virtexuplus",
        resources={"LUT": 1728000, "FF": 3456000, "DSP": 12288, "BRAM_18K": 5376, "URAM": 1280},
        memory="4 DDR banks; config/u250/link.cfg maps gmem0..2 to DDR[0..2]",
        default_clock_mhz=300,
        notes="Datacenter Alveo card; host and kernels share x86 build preset.",
    ),
    "u55c": PlatformInfo(
        name="u55c",
        part="xcu55c-fsvh2892-2L-e",
        family="virtexuplus",
        resources={"LUT": 1303680, "FF": 2607360, "DSP": 9024, "BRAM_18K": 2016, "URAM": 960},
        memory="HBM device; link.cfg controls bank/port placement",
        default_clock_mhz=300,
        notes="Datacenter Alveo HBM card; platform availability is site-specific.",
    ),
}


def get_platform_info(platform: str) -> PlatformInfo | None:
    return _PLATFORMS.get(platform.lower())
