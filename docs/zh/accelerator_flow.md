# 加速卡流程

这篇解释 U250、U50、U55C、U200、U280、VCK5000 这类 PCIe 加速卡的流程。这类卡通常插在 x86_64 主机里，host app 也在同一台机器上运行。

## 1. 加速卡有什么不同？

对加速卡来说：

- CPU host app 跑在工作站/服务器上
- XRT 也装在这台机器上
- FPGA 卡通过 PCIe 可见
- host app 直接加载 xclbin
- 正常 host build 不需要 PetaLinux sysroot

基本路径：

```text
CPU 测试
  ↓
csynth/cosim kernel
  ↓
为加速卡 platform 链接 xclbin
  ↓
构建 host app
  ↓
通过 XRT 在本机运行 host app
  ↓
比较输出
```

## 2. 准备环境

你需要：

1. Vitis，例如 `/tools/Xilinx/Vitis/2024.2`。
2. XRT，通常在 `/opt/xilinx/xrt`。
3. 对应加速卡的 platform `.xpfm`。
4. 如果要跑真实硬件，XRT 能看到加速卡。

典型 shell 设置：

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
. /opt/xilinx/xrt/setup.sh
xbutil examine
```

`xbutil examine` 应该列出卡。如果没有，先修 XRT/驱动/卡安装，不要先怀疑 Anvil。

## 3. 检查 target 配置

打开 `config/u250/anvil.mk` 或你的 target。重要字段：

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_PLATFORM       ?= /path/to/platform.xpfm
ANVIL_PRESET         := u250-host
ANVIL_HWEMU_PRESET   := u250-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim
```

含义：

- `ANVIL_PLATFORM` 是 Vitis platform 文件。
- `ANVIL_PRESET` 是硬件构建用的 CMake preset。
- `ANVIL_HWEMU_PRESET` 是 hardware emulation 用的 preset。
- `ANVIL_KERNEL_TARGETS` 定义 `KERNEL=all` 综合哪些 target。
- `ANVIL_COSIM_TARGETS` 定义 `KERNEL=all` cosim 哪些 target。

如果 platform 路径错，所有 Vitis 步骤都会很早失败。

## 4. 先跑 synthesis 和 cosim

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

先做这个再 link xclbin。kernel 问题在这里发现更快。

## 5. 理解 `link.cfg`

对加速卡，`config/<target>/link.cfg` 告诉 Vitis kernel 怎么连到内存。

例子：

```ini
[connectivity]
nk=saxpy:1:saxpy_1
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]

[clock]
freqHz=300000000:saxpy_1
```

含义：

- `nk=saxpy:1:saxpy_1` 从 top function `saxpy` 创建一个 compute unit，名字叫 `saxpy_1`。
- `sp=saxpy_1.x:DDR[0]` 把指针参数 `x` 绑定到 DDR bank 0。
- `freqHz=...` 给这个 compute unit 请求时钟。

host app 之后会按 compute-unit 名打开 kernel：

```cpp
ctx.GetKernel("saxpy:{saxpy_1}");
```

如果 `link.cfg` 里叫 `saxpy_1`，但 host 要 `saxpy_2`，运行会失败。

## 6. 链接 xclbin

```bash
make xclbin TARGET=u250
```

这一步可能很慢。产物类似：

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

如果 link 失败，看 Vitis link log。常见问题：

- `sp=` 参数名无效
- platform 不支持这个 memory bank 名
- kernel 太多或资源不够
- platform 版本和 Vitis 不匹配

## 7. 构建并运行 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

`run-host` 中发生的事：

1. host app 打开 device 0
2. 加载 xclbin
3. 找到 `saxpy_1` 这样的 compute unit
4. 分配 XRT buffer
5. 把输入拷到卡上
6. 启动 kernel
7. 把输出拷回
8. 把输出写到 `data/tiny/`

如果 `run-host` 失败，先判断失败发生在 kernel launch 前还是后：

- launch 前：XRT、device、xclbin、kernel 名问题
- launch 后：buffer group、数据布局、kernel 正确性或 compare 问题

## 8. Hardware emulation

Hardware emulation 使用仿真的设备。它比 CPU 测试慢，但不需要物理卡。

典型流程：

```bash
make xclbin-hwemu TARGET=u250
make build TARGET=u250 HOST_APP=run_saxpy
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

hw_emu 用来调 host/XRT 集成。它不能替代 csynth/cosim，因为它们回答的是不同问题。

## 9. Stream pipeline demo

部分加速卡 target 有 `config/<target>/pipeline_demo.cfg`，会启用 kernel-to-kernel stream demo：

```text
saxpy_stream → vadd_stream
```

显式构建：

```bash
make csynth TARGET=u250 KERNEL=pipeline_demo
make cosim TARGET=u250 KERNEL=pipeline_demo
make pipeline-demo TARGET=u250
make build TARGET=u250 HOST_APP=run_pipeline_demo
```

这是 opt-in 的。普通 `make build` 不会构建它，因为 stream pipeline synthesis/link 可能很慢。

## 10. 添加另一张加速卡

添加类似 PCIe 卡：

1. 复制接近的配置目录，例如 `config/u250` 到 `config/my_card`。
2. 编辑 `config/my_card/anvil.mk`。
3. 设置正确 `.xpfm` 路径。
4. 根据 platform 修改 `link.cfg` 的 memory bank 名。
5. 添加或复制 CMake presets。
6. 在 `tools/hlsflow/platform_info.py` 添加平台元数据，让报告显示资源百分比。
7. 先跑 `make csynth TARGET=my_card KERNEL=saxpy`，再尝试 xclbin。

不要假设 DDR/HBM bank 名在不同卡之间可移植。一定要查 platform 文档或该卡的 Vitis 示例。
