# 快速开始：把流程完整跑一遍

这篇按 FPGA 开发的正常顺序带你从头到尾跑一遍项目。目标不是追求性能，而是理解每一步在做什么，并确认你的环境能跑。

如果这些词还不熟悉：kernel、host app、xclbin、XRT、csynth、cosim、MODE，先读 [基本概念](concepts.md)。

## 你会跑哪几层

整个流程有四个层级。**从第一层开始，前一层通过了再往下走。**

| 层级 | 命令 | 需要 Vitis？ | 需要 XRT？ | 需要 FPGA 硬件？ | 目的 |
|---|---|:---:|:---:|:---:|---|
| CPU-only 测试 | `make test` | 否 | 否 | 否 | 验证普通 C++ 和 Python 代码 |
| HLS 综合 | `make csynth TARGET=... KERNEL=...` | 是 | 否 | 否 | 把 kernel C++ 转成硬件报告 |
| HLS cosim | `make cosim TARGET=... KERNEL=...` | 是 | 否 | 否 | 验证生成的 RTL 和 C++ kernel 一致 |
| 设备运行 | `make xclbin`，然后 `make swemu`/`make hwemu`/`make hw`/`make qemu` | 是 | 是 | 可选 | 构建并运行对应 mode 的 FPGA 二进制 |

**不要跳步骤。** 一个有 bug 的 kernel 在硬件里还是有 bug。在 `csynth` 或 `cosim` 阶段就抓住它，比等几小时 xclbin 构建完再发现要好得多。

---

## 第 1 步：检查仓库状态

```bash
git status --short
```

工作树干净时最容易调试。构建输出放 `build/`，数据集放 `data/`，运行输出放 `runs/`。这些都不提交到 git。

---

## 第 2 步：跑 CPU-only 测试

```bash
make test
```

这是最快的完整性检查。它配置一个 CPU-only CMake 构建，编译所有 CPU 侧代码（gold reference、HLS model、测试），然后跑 CTest 和 Python 测试套件。不需要 Vitis、XRT、FPGA 硬件。

期望输出：

```text
100% tests passed
```

**这一步失败就先修这里，不要去碰 Vitis。** CPU-only 失败几乎都是普通 C++ 或 Python 问题，不是 FPGA 特定的。

这一步内部做什么：
1. 配置 `hls-model-linux-debug` CMake preset（原生 x86_64 debug 构建）。
2. 构建 CPU 侧目标：gold、HLS model、测试、CLI 工具。
3. 跑 CTest（C++ 测试）和 pytest（Python 工具测试）。

---

## 第 3 步：认识 demo kernel 和目录结构

默认 demo 有三个 kernel：

| Kernel | 计算什么 | 顶层函数名 |
|---|---|---|
| `saxpy` | `out[i] = a * x[i] + y[i]` | `saxpy` |
| `vadd` | `out[i] = a[i] + b[i]` | `vadd` |
| `pipeline_demo` | `saxpy_stream → vadd_stream` kernel-to-kernel 流水线 | `saxpy_stream` + `vadd_stream` |

三个 host app：

| Host app | 跑什么 |
|---|---|
| `run_saxpy` | 加载包含 `saxpy_1` 的 xclbin |
| `run_vadd` | 加载包含 `vadd_1` 的 xclbin |
| `run_pipeline_demo` | 加载 stream pipeline xclbin |

**关键目录结构：**

```
src/kernels/include/kernels/   ← Kernel ABI 头文件和共享核心 helper（编辑这里）
src/kernels/                   ← Vitis HLS kernel 实现（编辑这里）
src/hls_model/                 ← CPU 编译的模型（结构镜像 kernel）（编辑这里）
src/gold/                      ← 简单 CPU 真值实现（编辑这里）
src/host/                      ← XRT host application（编辑这里）
include/anvil/hls/             ← 框架 hlslib wrapper（不要编辑）
config/<target>/               ← 板卡/platform 配置（编辑这里）
```

---

## 第 4 步：生成数据集和 gold 参考输出

生成输入数据集：

```bash
make gen DATASET=tiny
```

这会把文件写到 `data/tiny/`：`meta.json`、`x.bin`、`y.bin` 等，具体内容取决于 kernel。只需跑一次，除非你改了 generator 或需要不同大小。

生成期望输出（CPU 真值）：

```bash
make gold DATASET=tiny
```

这跑标量 CPU gold reference，写 `data/tiny/gold_out.bin`。在 `gen` 之后跑。

之后用 FPGA 的输出和这个 gold 对比。

---

## 第 5 步：跑 HLS 综合

选择一个 target。加速卡用户，安装了 Vitis 2024.2 的话，U250 是常见选择：

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

这读取 `config/u250/anvil.mk` 找到 Vitis platform 和 part，配置 CMake preset，然后对 `saxpy` kernel 跑 Vitis HLS compile mode。综合要几分钟。

**重要产物：**

- HLS 报告（在 build tree 下）：
  ```text
  build/u250-host/src/kernels/saxpy_hls/hls/syn/report/saxpy_csynth.xml
  ```
- `.xo` kernel object：`saxpy.xo`

读综合报告：

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

重点看：
- **II（Initiation Interval）** — 流水线接受相邻输入之间的时钟周期数。1 是理想值。比 1 大意味着吞吐量降低。pragma 请求值和实际达到值都会显示出来。
- **Timing** — 设计能否在时钟周期内跑完。
- **资源利用率** — LUT/FF/DSP/BRAM/URAM 的用量。

这时候还没在 FPGA 上跑，只是在生成硬件报告。

**综合失败时：**
- 查 Vitis HLS log 里的错误信息。
- 常见原因：kernel 用了 HLS 不支持的 C++（dynamic allocation、virtual function、复杂模板），include 路径错，platform 路径不存在。

---

## 第 6 步：跑 HLS cosim

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosim 用 `tests/kernels/` 下的 C++ testbench 驱动综合生成的 RTL。它验证 RTL 产生的结果和 C++ kernel 一致。Cosim 比综合慢。

**Cosim 失败时：**
- 不要先去查 host app。问题在 kernel 或 testbench。
- 检查 testbench 输入是否符合 kernel 的假设（元素数、pack 对齐、标量参数）。
- 查 cosim log 里的具体仿真失败位置。

---

## 第 7 步：链接 xclbin

```bash
make xclbin TARGET=u250
```

Vitis linker 把综合好的 kernel object（`saxpy.xo`、`vadd.xo`）使用 platform 和 `config/u250/link.cfg` 生成 FPGA 二进制。对硬件构建来说这通常是最慢的步骤。

Xclbin 输出在：

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

查看 link 结果：

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

这会显示：
- Xclbin 是否生成
- 里面有哪些 compute unit（如 `saxpy_1`、`vadd_1`）
- 每个指针参数绑定到哪个内存 bank（如 `saxpy_1.x → DDR[0]`）
- 时钟设置
- Vitis link warning 或 error

**在构建 host app 之前先修这里的问题。** Xclbin 里 compute unit 名或内存绑定错了，host app 在运行时无法修复。

---

## 第 8 步：构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

这只编译选中的 host 可执行文件，不跑 HLS 综合，不创建 Python 环境。输出：

```text
build/u250-host/src/host/run_saxpy
```

对需要交叉编译的嵌入式 target，见 [Embedded 流程](embedded_flow.md)。

---

## 第 9 步：在硬件或 emulation 上运行

跑 emulation 前，先生成 emulation 配置文件：

```bash
make emconfig TARGET=u250 MODE=sw_emu
```

这生成 `build/u250/emconfig/emconfig.json`。同一 target 的 `sw_emu` 和 `hw_emu` 共用这个文件，每个 target 只需生成一次。

### Software emulation（无需加速卡，速度快）

Software emulation 用模拟的 XRT 环境跑原始 C++ kernel 代码。用来在硬件综合前先验证 host app 逻辑是否正确。

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Software emulation 不需要先跑 csynth。它用 kernel 的软件模型，所以看不出 timing 或 RTL 问题。

### Hardware emulation（无需加速卡，速度慢）

Hardware emulation 对综合好的 kernel 跑 RTL 仿真。比 software emulation 慢很多，但能抓住 RTL 层的 bug。

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Hardware emulation 需要先跑 csynth。用非常小的数据集 — 大数据的 RTL 仿真可能要跑几小时。

### 真实硬件运行

如果机器上装了 FPGA 加速卡并且 XRT 已 source：

```bash
. /opt/xilinx/xrt/setup.sh
xbutil examine    # 确认加速卡可见
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

---

## 第 10 步：比较结果

任何运行（swemu、hwemu、hw）结束后，把输出和 gold reference 对比：

```bash
make compare DATASET=tiny
```

所有 run mode 的输出都写到 `runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/`，compare 从那里读取。

Compare 通过说明 FPGA（或 emulation）输出了正确结果。

---

## 第 11 步：嵌入式板卡

嵌入式 target（ZCU102、ZCU104、KV260 等）需要额外两样东西：

- ARM host app 的交叉编译 sysroot
- 带 XRT 的板卡文件系统

```bash
export PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux

make build-host TARGET=zcu102 HOST_APP=run_saxpy  # 为 AArch64 交叉编译
make csynth     TARGET=zcu102 KERNEL=saxpy
make xclbin     TARGET=zcu102
make gen        DATASET=tiny
make gold       DATASET=tiny
```

然后部署并运行。见 [Embedded 流程](embedded_flow.md) 和 [部署指南](deploy.md)。

嵌入式 target 的 QEMU emulation：

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

Launcher 脚本是 platform/image 特定的，必须在 `$OUTPUT` 路径写出输出文件。

---

## 怎样算成功

第一次完整跑通应该满足：

- `make test` 通过（CPU 代码正确）
- 至少一个 kernel 通过 `csynth`（Vitis 接受代码）
- 同一个 kernel 通过 `cosim`（RTL 和 C++ kernel 一致）
- 一个 host app 能用 `make build` 构建
- `make swemu` 或 `make hw` 产生的输出通过 `make compare`

之后读 [自定义指南](customization.md)，把 demo 换成你自己的算法。

---

## 第一次运行常见问题

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| `make test` 失败 | 普通 C++/Python bug | 读 CTest 或 pytest 的错误信息 |
| `vitis_hls: command not found` | Vitis 没有 source | `. /tools/Xilinx/Vitis/2024.2/settings64.sh` |
| platform not found | `config/<target>/anvil.mk` 里 `.xpfm` 路径不存在 | 检查路径是否在磁盘上存在 |
| csynth 通过但 cosim 失败 | RTL 和 C++ kernel 有差异 | 调试 testbench 和 kernel 逻辑 |
| `xbutil examine` 没有显示加速卡 | XRT 没有 source 或卡未安装 | `. /opt/xilinx/xrt/setup.sh`，检查 PCIe 连接 |
| Host app 报 "cannot open xclbin" | Xclbin 路径错或 MODE 不匹配 | 确认对应 mode 的 xclbin 文件存在 |
| Host app 报 "kernel not found" | compute unit 名不匹配 | 检查 `link.cfg` `nk=` 和 host `GetKernel()` 的字符串是否一致 |
| 输出和 gold 不匹配 | Kernel bug、BO group index 或数据布局问题 | 先跑 `make swemu` 缩小范围，再和 gold 逐字节对比 |
