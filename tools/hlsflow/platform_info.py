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
    "zcu106": PlatformInfo(
        name="zcu106",
        part="xczu7ev-ffvc1156-2-e",
        family="zynquplus",
        resources={"LUT": 230400, "FF": 460800, "DSP": 1728, "BRAM_18K": 624, "URAM": 0},
        memory="PS DDR via HP/HPC ports; Vitis base platform exposes DDR banks",
        default_clock_mhz=200,
        notes="ZCU106 uses the ZU7EV device family; resource totals mirror ZCU104-class ZU7EV metadata.",
    ),
    "kv260": PlatformInfo(
        name="kv260",
        part="xck26-sfvc784-2lv-c",
        family="zynquplus",
        # AMD DS987 K26 SOM PL resources: 117,120 CLB LUTs, 234,240 CLB FFs,
        # 144 36Kb BRAM blocks, 64 URAM blocks, 1,248 DSP slices.
        # hlsflow reports BRAM_18K, so convert 144 x 36Kb blocks -> 288 x 18Kb.
        resources={"LUT": 117120, "FF": 234240, "DSP": 1248, "BRAM_18K": 288, "URAM": 64},
        memory="4 GB LPDDR4 on the K26 SOM; embedded Vitis platform exposes PS DDR ports",
        default_clock_mhz=200,
        notes="Kria KV260 Vision AI Starter Kit / K26 SOM; host is AArch64 cross-compiled.",
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
    "u50": PlatformInfo(
        name="u50",
        part="xcu50-fsvh2104-2-e",
        family="virtexuplus",
        # UG1120 U50 XDMA base_5 dynamic region resources after static shell:
        # SLR0+SLR1 = 351K+353K LUT, 703K+707K registers, 552+564 BRAM tiles,
        # 272+272 URAM, 2352+2568 DSP. Exact availability can vary by platform shell.
        resources={"LUT": 704000, "FF": 1410000, "DSP": 4920, "BRAM_18K": 1116, "URAM": 544},
        memory="8 GB HBM2, 32 HBM pseudo channels; PLRAM available per SLR",
        default_clock_mhz=300,
        notes="Datacenter Alveo U50; resources are platform dynamic-region availability, not raw FPGA total.",
    ),
    "u200": PlatformInfo(
        name="u200",
        part="xcu200-fsgd2104-2-e",
        family="virtexuplus",
        # UG1120 U200 XDMA dynamic region resources, summed across SLR0/1/2:
        # LUT 385K+200K+383K; registers 771K+399K+765K; BRAM 720+360+720;
        # URAM 320+160+320; DSP 2280+1320+2280. Exact availability varies by shell.
        resources={"LUT": 968000, "FF": 1935000, "DSP": 5880, "BRAM_18K": 1800, "URAM": 800},
        memory="4 DDR banks; one bank may be static-region constrained depending on platform shell",
        default_clock_mhz=300,
        notes="Datacenter Alveo U200; resources are platform dynamic-region availability.",
    ),
    "u280": PlatformInfo(
        name="u280",
        part="xcu280-fsvh2892-2L-e",
        family="virtexuplus",
        # UG1120 U280 XDMA platform resources, summed across SLR0/1/2:
        # LUT 369K+334K+367K; registers 746K+677K+728K; BRAM 507+468+512;
        # URAM 320+320+320; DSP 2733+2877+2880. Exact availability varies by shell.
        resources={"LUT": 1070000, "FF": 2151000, "DSP": 8490, "BRAM_18K": 1487, "URAM": 960},
        memory="8 GB HBM2, 32 HBM pseudo channels plus DDR/PLRAM on older shells",
        default_clock_mhz=300,
        notes="Datacenter Alveo U280; resources are platform dynamic-region availability.",
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
    "vck5000": PlatformInfo(
        name="vck5000",
        part="xcvc1902-vsva2197-2MP-e-S",
        family="versal",
        # Public VCK5000 docs list about 899,840 LUTs, 1,968 DSPs, 4.2 MB BRAM,
        # and 16.3 MB URAM. BRAM_18K/URAM entries below are approximate block
        # conversions for utilization context; Vitis XML should override when present.
        resources={"LUT": 899840, "DSP": 1968, "BRAM_18K": 1911, "URAM": 464},
        memory="16 GB LPDDR4; Versal AI Engine + programmable logic acceleration card",
        default_clock_mhz=300,
        notes="Versal VCK5000; BRAM/URAM block counts are derived from published memory MB figures.",
    ),
}


def get_platform_info(platform: str) -> PlatformInfo | None:
    return _PLATFORMS.get(platform.lower())
