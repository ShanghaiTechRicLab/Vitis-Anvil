# 自定义指南

把 Vitis-Anvil 当模板使用：保留流程，替换 demo 部分。

## 1. 添加或替换 kernel

Kernel source 位于 `src/kernels/`。在 `src/kernels/CMakeLists.txt` 注册 kernel：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

如果有 cosim testbench：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  TESTBENCH     ${CMAKE_SOURCE_DIR}/tests/kernels/my_kernel_cosim_tb.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

运行：

```bash
make csynth TARGET=u250 KERNEL=my_kernel
make cosim TARGET=u250 KERNEL=my_kernel
```

## 2. 添加 host app

Host apps 位于 `src/host/`。在 `src/host/CMakeLists.txt` 添加 executable target，链接需要的库，然后用 `HOST_APP=` 运行：

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

运行时 helper 示例：

```cpp
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/xrt_buffer.hpp>

anvil::runtime::XrtContext ctx{0, xclbin_path};
auto kernel = ctx.GetKernel("my_kernel:{my_kernel_1}");
```

## 3. 添加 xclbin connectivity

添加或修改 `config/<target>/link.cfg`：

```ini
[connectivity]
nk=my_kernel:1:my_kernel_1
sp=my_kernel_1.input:DDR[0]
sp=my_kernel_1.output:DDR[1]

[clock]
freqHz=300000000:my_kernel_1
```

在 `src/kernels/CMakeLists.txt` 注册 xclbin：

```cmake
add_anvil_xclbin(
  NAME my_kernel
  KERNEL_TARGETS my_kernel_xo
  LINK_CFG ${CMAKE_SOURCE_DIR}/config/u250/link.cfg
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

## 4. 添加设备

创建 `config/my_board/anvil.mk`：

```make
ANVIL_DEVICE_KIND      := accelerator
ANVIL_VITIS_PART       := xcu250-figd2104-2L-e
ANVIL_PLATFORM         := /path/to/platform.xpfm
ANVIL_PRESET           := my-board-host
ANVIL_HWEMU_PRESET     := my-board-host-hwemu
ANVIL_NEEDS_CROSS      := no
ANVIL_XCLBIN_MODE      := hw
ANVIL_XRT_LIB          := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS   := my_kernel_xo
ANVIL_COSIM_TARGETS    := my_kernel_cosim
```

然后在 `CMakePresets.json` 添加对应 configure/build/test presets。Embedded 设备通常需要：

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

## 5. 添加平台元数据

`hlsflow` 的设备说明位于 `tools/hlsflow/platform_info.py`。添加资源总量、memory notes、default clock 后，报告里会显示更有用的 headroom 信息。

## 6. 自定义数据集和 gold references

数据生成：

- `scripts/gen_dataset.py`
- 输出到 `data/<dataset>/`

Gold references：

- C++：`src/gold/cpp/`
- Python：`src/gold/python/`

比较逻辑：

- `scripts/compare.py`

保持 contract 简单：host app 把输出写到 `data/<dataset>/`，`make compare` 把它和 gold output 比较。

## 7. 自定义阈值

通过 Make 使用 `hlsflow check`：

```bash
make check-hls
```

或直接运行：

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000
```

## 8. 保持命令语义干净

- `TARGET=` 选择 board/platform。
- `KERNEL=` 选择 HLS kernel targets 和 HLS reports。
- `HOST_APP=` 选择 XRT host executables。
- `DATASET=` 选择输入/输出数据集。

不要用 `HOST_APP` 选择 cosim；cosim 是 kernel/testbench 级别。
