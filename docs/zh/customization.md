# 自定义指南

Vitis-Anvil 本质是一个模板。保留构建流程和工具链，把 demo 部分换成你自己的 kernel 和 host app。

## 1. 添加或替换 kernel

Kernel 源码放在 `src/kernels/`。在 `src/kernels/CMakeLists.txt` 里注册：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

如果有 cosim testbench，加上 `TESTBENCH`：

```cmake
add_anvil_kernel(
  NAME          my_kernel
  TOP           my_kernel
  SOURCES       my_kernel.cpp
  TESTBENCH     ${CMAKE_SOURCE_DIR}/tests/kernels/my_kernel_cosim_tb.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})
```

然后运行：

```bash
make csynth TARGET=u250 KERNEL=my_kernel
make cosim TARGET=u250 KERNEL=my_kernel
```

## 2. 添加 host app

Host apps 放在 `src/host/`。在 `src/host/CMakeLists.txt` 里添加 executable target，链接你需要的库，构建时用 `HOST_APP=` 选择：

```bash
make build TARGET=u250 HOST_APP=run_my_kernel
make run-host TARGET=u250 HOST_APP=run_my_kernel DATASET=tiny
```

你可能需要的运行时 helper：

```cpp
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/xrt_buffer.hpp>

anvil::runtime::XrtContext ctx{0, xclbin_path};
auto kernel = ctx.GetKernel("my_kernel:{my_kernel_1}");
```

## 3. 添加 xclbin connectivity

编辑或创建 `config/<target>/link.cfg`：

```ini
[connectivity]
nk=my_kernel:1:my_kernel_1
sp=my_kernel_1.input:DDR[0]
sp=my_kernel_1.output:DDR[1]

[clock]
freqHz=300000000:my_kernel_1
```

在 `src/kernels/CMakeLists.txt` 里注册 xclbin：

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

然后在 `CMakePresets.json` 里添加对应的 configure/build/test presets。

嵌入式设备：

```make
ANVIL_DEVICE_KIND := embedded
ANVIL_NEEDS_CROSS := yes
ANVIL_SYSROOT ?= $(PETALINUX_SYSROOT)
ANVIL_HOST_PRESET := my-board-host
```

## 5. 添加平台元数据

`hlsflow` 的设备信息放在 `tools/hlsflow/platform_info.py`。加上资源总量、内存说明和默认时钟后，报告里会显示更直观的余量信息。

## 6. 自定义数据集和 golden reference

数据生成：

- `scripts/gen_dataset.py` — 定义数据格式
- 输出到 `data/<dataset>/`

Golden reference：

- C++：`src/gold/cpp/`
- Python：`src/gold/python/`

比较逻辑：

- `scripts/compare.py` — 把硬件输出和 gold 比较

保持约定简单：host app 把输出写到 `data/<dataset>/`，`make compare` 拿这些文件和 golden reference 对比。

## 7. 自定义阈值

通过 Make 使用 `hlsflow check`：

```bash
make check-hls
```

或者直接运行，自己设定阈值：

```bash
PYTHONPATH=tools .venv/bin/python -m hlsflow check --max-ii 1 --max-lut 200000 --max-dsp 1000
```

## 8. 保持命令语义清晰

- `TARGET=` 选择板卡或 platform
- `KERNEL=` 选择 HLS kernel 目标和 HLS 报告
- `HOST_APP=` 选择 XRT host 可执行程序
- `DATASET=` 选择输入和输出数据集

不要用 `HOST_APP` 来选择 cosim 或 kernel。Cosim 是 kernel 级别的事情，属于 `KERNEL` 的职责范围。
