# 加速卡流程

这篇讲 PCIe 加速卡的流程：U250、U50、U55C、U200、U280、VCK5000 等 Alveo 系列卡。这类卡插在 PCIe 插槽里，host app 也跑在同一台 x86_64 机器上。

---

## 1. 加速卡和嵌入式板卡有什么区别？

| 方面 | 加速卡 | 嵌入式板卡 |
|---|---|---|
| Host app 跑在哪 | 同一台 x86_64 机器 | 板卡上的 ARM CPU（AArch64） |
| XRT 在哪 | Host 机器 | 板卡镜像 |
| Host 编译 | 原生 x86_64 | 需要 sysroot 交叉编译 |
| 数据传输 | PCIe DMA | 片上互联 |
| 需要部署步骤 | 不需要 | 需要 SSH 拷贝 |

如果你的板卡是 ZCU102、ZCU104 或 KV260，读 [Embedded 流程](embedded_flow.md)。

---

## 2. 准备环境

跑任何 Vitis 命令前，先安装并 source 这些工具：

```bash
# source Vitis（调整版本和路径匹配你的安装）
. /tools/Xilinx/Vitis/2024.2/settings64.sh

# source XRT
. /opt/xilinx/xrt/setup.sh

# 确认加速卡可见
xbutil examine
```

`xbutil examine` 应该列出你安装的 Alveo 卡及其 BDF 地址（如 `[0000:65:00.1]`）。如果没有出现，先修 XRT/驱动安装，不要先怀疑 Anvil。

还需要加速卡的 platform `.xpfm` 文件。常见路径：

| 卡 | 典型 platform 路径 |
|---|---|
| U250 | `/opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/` |
| U50 | `/opt/xilinx/platforms/xilinx_u50_gen3x4_xdma_2_202020_1/` |
| U55C | `/opt/xilinx/platforms/xilinx_u55c_gen3x16_xdma_3_202210_1/` |
| U280 | `/opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/` |

---

## 3. 检查 target 配置

打开 `config/u250/anvil.mk`（或你的 target 的配置文件）：

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_VITIS_PART     := xcu250-figd2104-2L-e
ANVIL_PLATFORM       ?= /opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1/xilinx_u250_gen3x16_xdma_4_1_202210_1.xpfm
ANVIL_PRESET         := u250-host
ANVIL_HWEMU_PRESET   := u250-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_XRT_LIB        := /opt/xilinx/xrt
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim
```

**最重要的一行是 `ANVIL_PLATFORM`。** 这个路径如果不在磁盘上，所有 Vitis 命令都会立即失败。需要时可以从命令行覆盖：

```bash
make csynth TARGET=u250 KERNEL=saxpy ANVIL_PLATFORM=/my/path/platform.xpfm
```

---

## 4. 先跑综合和 cosim——永远先做这步

不要直接跑 xclbin link。综合和 cosim 是便宜的检查，能在慢的 link 步骤之前抓住 kernel bug。

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze TARGET=u250 KERNEL=saxpy    # 读综合报告
make cosim  TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

看 `analyze` 输出时关注：
- **II=1**：流水线每个时钟周期接受一个新输入。II>1 降低吞吐量。
- **Timing**：设计必须在时钟周期内完成。负 slack 意味着 timing 失败。
- **资源**：LUT/FF/DSP/BRAM/URAM 用量。DSP 用量高通常意味着有展开的乘法运算。

---

## 5. 理解 link.cfg

`config/<target>/link.cfg` 告诉 Vitis 怎么把 kernel compute unit 连到卡上的内存 bank。

```ini
[connectivity]
# 从 kernel 顶层函数创建 compute unit
# 格式：nk=<顶层函数名>:<数量>:<实例名>
nk=saxpy:1:saxpy_1
nk=vadd:1:vadd_1

# 把 kernel 指针参数绑定到内存 bank
# 格式：sp=<实例名>.<参数名>:<内存 bank>
# 只有指针参数需要 sp= 条目。标量参数不需要。
sp=saxpy_1.x:DDR[0]
sp=saxpy_1.y:DDR[1]
sp=saxpy_1.out:DDR[2]
sp=vadd_1.a:DDR[0]
sp=vadd_1.b:DDR[1]
sp=vadd_1.out:DDR[2]

[clock]
# 给 compute unit 请求时钟频率
# 格式：freqHz=<频率（Hz）>:<实例名>
freqHz=300000000:saxpy_1
freqHz=300000000:vadd_1
```

**关键规则：**
- `nk=` 的函数名必须和 C++ `extern "C"` 函数名完全一致。
- `sp=` 的参数名必须和 kernel 函数参数名完全一致。如果 kernel 是 `void saxpy(const float* x, ...)`，那么 `sp=` 里写 `saxpy_1.x:DDR[0]` — 不是 `in`，不是 `a`。
- **标量参数**（`alpha`、`n_packs` 等）是控制寄存器参数，不写 `sp=` 条目。
- 内存 bank 名（`DDR[0]`、`HBM[0:3]`）是 platform 相关的。U250 有 DDR bank；U50 有 HBM bank；两者不通用。
- `nk=` 里的实例名（如 `saxpy_1`）必须和 host app 的 `GetKernel()` 调用一致：

```cpp
ctx.GetKernel("saxpy:{saxpy_1}");  // host app 使用 nk= 里的实例名
```

**常见 link.cfg 错误：**
- `sp=saxpy_1.in` 但 kernel 参数名是 `x` — 名字必须完全一致
- `sp=saxpy_1.alpha:DDR[0]` — 标量参数没有内存绑定
- 在只有 DDR 的卡（如 U250）上用 `HBM[0]`

---

## 6. 链接 xclbin

```bash
make xclbin TARGET=u250
```

产物在：

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

link 完之后，**在碰 host app 之前**先查看 xclbin 里实际有什么：

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

这显示 compute unit 名、内存端口绑定、时钟频率、任何 link warning。**在这里修，不是在 host app 里修。** Xclbin 里没有 `saxpy_1`，host app 没法修复。

Hardware xclbin 构建很慢。Software/hardware emulation xclbin 构建更快但也要几分钟。

---

## 7. 构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

产物：`build/u250-host/src/host/run_saxpy`。

这一步只编译 C++，不综合 kernel。速度快。

---

## 8. 生成数据和 gold

```bash
make gen  DATASET=tiny
make gold DATASET=tiny
```

每个数据集跑一次。在 `data/tiny/` 下创建输入文件和期望输出。

---

## 9. 在真实硬件上运行

```bash
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

这做什么：
1. Host app 通过 XRT 打开 device 0
2. 加载 xclbin
3. 找到 compute unit `saxpy_1`
4. 分配 XRT buffer object（BO）并映射到 DDR bank
5. 把 `data/tiny/` 里的输入数据拷到 DDR
6. 启动 kernel
7. 把 DDR 输出拷回 host
8. 把输出写到 `runs/u250/hw/run_saxpy/tiny/<run_key>/`

然后和 gold 对比：

```bash
make compare DATASET=tiny
```

**硬件运行失败时：**
- Kernel launch 前失败：XRT、device、xclbin 路径或 compute unit 名问题
- Kernel launch 后失败：buffer group index、数据布局、kernel 正确性或结果格式问题

---

## 10. Software 和 hardware emulation

Emulation 让你在没有物理加速卡的情况下测试 host/XRT 集成。

### 什么情况用哪个 mode？

| | sw_emu | hw_emu |
|---|---|---|
| 仿真的是什么 | Kernel 的快速 C 行为模型 | RTL 仿真（Vivado xsim） |
| 速度 | 几分钟 | 非小数据可能要几小时 |
| 能抓住什么 | Host app API bug、BO 设置、参数传递 | RTL 正确性、接口 timing、内存协议 |
| 需要先跑 csynth？ | 否 | 是 |
| 数据集大小 | 正常大小没问题 | 只用非常小的数据集 |

### 跑 emulation

生成 emulation 配置文件（每个 target 生成一次）：

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

跑 software emulation：

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

跑 hardware emulation：

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

两条命令都会自动设置 `XCL_EMULATION_MODE` 和 `EMCONFIG_PATH`。运行输出分别在 `runs/u250/sw_emu/` 和 `runs/u250/hw_emu/`。

和 gold 对比：

```bash
make compare DATASET=tiny
```

**Emulation 报 "cannot find emconfig"：**
检查 `build/u250/emconfig/emconfig.json` 是否存在。不存在就跑 `make emconfig TARGET=u250 MODE=sw_emu`。

**hw_emu 结果错但 sw_emu 对：**
RTL 有 bug 但 C++ 模型没有发现。调试 kernel 并重跑 cosim。

**sw_emu 结果错但 gold 对：**
Host app 有 bug（BO 设置、参数传递、元素数 vs pack 数等）。

---

## 11. xrt.ini：运行时 profiling 和追踪

`config/<target>/xrt.ini` 控制 XRT profiling 和 debug 输出。示例：

```ini
[Runtime]
verbosity = 5

[Debug]
timeline_trace = true
data_transfer_trace = coarse
```

XRT 在 host 可执行文件的同级目录找这个文件。本地运行时可手动拷贝，或设置 `XILINX_XRT_INI` 环境变量指向它。

---

## 12. Stream pipeline demo（可选）

部分加速卡配置里有 `config/<target>/pipeline_demo.cfg`，启用 kernel-to-kernel stream demo：

```text
saxpy_stream → vadd_stream
```

显式构建和运行：

```bash
make csynth TARGET=u250 KERNEL=pipeline_demo
make cosim  TARGET=u250 KERNEL=pipeline_demo
make xclbin TARGET=u250   # （有 pipeline_demo.cfg 时会用它）
make build  TARGET=u250 HOST_APP=run_pipeline_demo
make hw     TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny
```

这是 opt-in 的。普通构建不包括它，因为 stream pipeline 综合/link 很慢。

---

## 13. 添加新的加速卡 target

添加类似 PCIe 卡：

1. 复制最接近的现有配置：`cp -r config/u250 config/my_card`
2. 编辑 `config/my_card/anvil.mk`：
   - 设置 `ANVIL_VITIS_PART` 为 FPGA part 号
   - 设置 `ANVIL_PLATFORM` 为 `.xpfm` 路径
3. 编辑 `config/my_card/link.cfg`：
   - 根据新 platform 更新内存 bank 名。U50 用 HBM bank，U250 用 DDR bank，不通用。
4. 在 `CMakePresets.json` 里给新 target 加 CMake preset（复制并改名 `u250-host`）。
5. 在 `tools/hlsflow/platform_info.py` 加平台元数据，让 `make analyze` 显示正确的资源百分比。
6. 先测综合再 link：

```bash
make csynth TARGET=my_card KERNEL=saxpy
make analyze TARGET=my_card KERNEL=saxpy
```

**不要假设内存 bank 名在不同卡之间可以直接移植。** 一定要查 platform 文档或该卡的 Vitis 示例。

---

## 14. 加速卡问题排查

### `xbutil examine` 没有显示加速卡

- 驱动没装：查 `dmesg | grep -i xdma` 或 `dmesg | grep -i xocl`
- 卡没插好
- XRT 版本和卡固件不匹配：跑 `xbutil program --update`

### csynth 通过但 xclbin link 失败

常见原因：
- `sp=` 参数名和 kernel 参数名不匹配
- 这个 platform 不支持该 memory bank 名
- Compute unit 太多超过器件资源
- Platform 版本和 Vitis 安装不匹配

### Host app 报 "xclbin not found"

检查 `make xclbin TARGET=<t>` 是否为正确的 `MODE` 完成了构建。`hw` run 需要 `hw` xclbin，不是 `hw_emu` 的。

### Host app 报 "kernel not found" 或 "CU not found"

Host app 里的 `ctx.GetKernel("saxpy:{saxpy_1}")` 和 `link.cfg` 里的 `nk=saxpy:1:saxpy_1` 实例名不一致。改其中一处让两边匹配。

### Buffer 分配失败

检查 BO group index。传给 `xrt::bo`（或 `kernel.group_id(arg_index)`）的 index 必须对应正确的内存 bank。参数 index 0 是 kernel 签名里第一个指针参数。

### 输出错误但没有报错

按这个顺序检查：
1. Host app 里的 BO group index
2. Kernel 参数顺序（host 传参顺序和 C++ 签名一致吗？）
3. `link.cfg` `sp=` 绑定
4. pack count vs element count（kernel 通常需要 `n_packs` 不是原始元素数）
5. 尾部 pack 补零处理（不被 pack 宽度整除的元素数需要特殊处理）
6. 数据集文件 — 是不是读了正确的输入文件？
