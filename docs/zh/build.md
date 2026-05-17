# 构建指南：每条 Make 命令详解

这篇解释每一个 Make target，每个条目说明它消耗什么、生成什么、什么时候用。构建缓存背后的模型见 [构建系统模型](build_system.md)。

## 五大变量

几乎每条命令都接受这几个变量的某种组合：

```bash
make <target> TARGET=u250 KERNEL=saxpy HOST_APP=run_saxpy DATASET=tiny MODE=hw
```

| 变量 | 选择什么 | 示例值 | 默认值 |
|---|---|---|---|
| `TARGET` | 板卡/platform 配置（`config/<target>/anvil.mk`） | `u250`, `u55c`, `zcu102`, `kv260` | `u250` |
| `KERNEL` | HLS kernel 目标名 | `saxpy`, `vadd`, `pipeline_demo`, `all` | `all` |
| `HOST_APP` | `src/host/` 下的 CPU 可执行文件名 | `run_saxpy`, `run_vadd` | `run_saxpy` |
| `DATASET` | `data/` 下的数据集目录名 | `tiny` | `tiny` |
| `MODE` | 执行上下文 | `hw`, `hw_emu`, `sw_emu` | `hw` |

先确认 `TARGET` 正确。`TARGET` 错了会用错 platform 路径、preset 和内存 bank 名。

---

## 快速 CPU-only targets

这些不需要 Vitis 或 XRT，先跑它们。

### `make test`

跑完整的 CPU-only 测试套件。

```bash
make test
```

**做什么：**
1. 配置 `hls-model-linux-debug` CMake preset（原生 x86_64 debug 构建）。
2. 构建 CPU 侧代码：gold reference、HLS model、测试、CLI 工具。
3. 跑 CTest（C++ 测试）。
4. 跑 pytest（Python 工具测试）。

**产物：**
- 构建产物在 `build/hls-model-linux-debug/`
- 测试通过/失败结果输出到终端

**什么时候用：**
- 修改任何 C++、Python 或数据格式代码之后
- 每次跑 HLS 综合或 cosim 之前
- 每次 `git pull` 之后第一件事

这一步失败就先修这里，再去用 Vitis。

---

## 数据集和 gold targets

### `make gen DATASET=<name>`

生成输入数据集。

```bash
make gen DATASET=tiny
```

**做什么：** 跑 `scripts/gen_dataset.py`（或编译后的 app），把二进制输入文件和 `meta.json` 写到 `data/<name>/`。

**产物：**
```text
data/tiny/meta.json    ← 数据集参数
data/tiny/x.bin        ← 输入数组
data/tiny/y.bin        ← 输入数组（如果 kernel 需要两个输入）
```

跑一次就够。只有改了 generator 或需要不同数据集时才重跑。

### `make gold DATASET=<name>`

跑 CPU gold reference，写入期望输出。

```bash
make gold DATASET=tiny
```

**依赖：** 同 dataset 的 `make gen`。

**产物：**
```text
data/tiny/gold_out.bin  ← kernel 的期望输出
```

所有 FPGA 运行结果都和这个文件对比。

---

## HLS 综合 targets

### `make csynth TARGET=<target> KERNEL=<kernel>`

把 kernel C++ 转成 HLS 输出：报告、资源估算、编译后的 kernel object。

```bash
make csynth TARGET=u250 KERNEL=saxpy
make csynth TARGET=u250 KERNEL=all      # 综合所有 kernel
```

**消耗：**
- Kernel 源文件：`src/kernels/<kernel>_kernel.cpp`
- ABI 头文件：`src/kernels/include/kernels/`
- Platform：`config/<target>/anvil.mk` 里的路径

**产物：**
- 带综合 log 的 HLS 工作目录
- `.xo` kernel object（编译后的 kernel）
- `<kernel>_csynth.xml` 报告（包含 II、timing、资源估算）

**失败时：**
- 查 Vitis HLS log。常见错误：
  - Kernel 用了 HLS 不支持的 C++ 写法（动态内存、虚函数、复杂模板）
  - Include 路径错或头文件不存在
  - `anvil.mk` 里 platform 路径不存在

### `make analyze TARGET=<target> KERNEL=<kernel>`

读现有 csynth 报告，展示易读的摘要。

```bash
make analyze TARGET=u250 KERNEL=saxpy
make analyze TARGET=u250 KERNEL=all
```

不跑综合。只读 build tree 下已有的报告。

**显示内容：**
- 达到的 II（Initiation Interval）vs. 目标 II
- 估算时钟周期和 timing 余量
- LUT、FF、DSP、BRAM、URAM 利用率（数量和占器件总量的百分比）
- II 违例或 timing 失败的警告

`csynth` 之后立即用这条命令。如果 II 或 timing 严重偏离目标，不要继续跑 cosim 或 xclbin。

---

## HLS cosim targets

### `make cosim TARGET=<target> KERNEL=<kernel>`

跑 C/RTL 协同仿真：C++ testbench 驱动综合后的 RTL。

```bash
make cosim TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=all
```

**消耗：**
- 综合后的 kernel（必须先跑 `csynth`）
- Testbench：`tests/kernels/<kernel>_cosim_tb.cpp`

**产物：**
- Cosim log 和报告（在 HLS 工作目录下）
- 通过/失败状态
- 事务级 latency 统计

**Cosim 是 kernel 测试。** 它不跑 host app，不加载 xclbin，不使用 XRT。Cosim 失败就修 kernel 或 testbench，不要继续往下走。

### `make analyze-cosim TARGET=<target> KERNEL=<kernel>`

读 cosim 报告，展示摘要。

```bash
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

显示通过/失败、latency、仿真错误。

---

## xclbin targets

### `make xclbin TARGET=<target>`

把综合好的 kernel object 链接成完整 FPGA 二进制。

```bash
make xclbin TARGET=u250             # 硬件 xclbin（默认 MODE=hw）
make xclbin TARGET=u250 MODE=hw_emu
make xclbin TARGET=u250 MODE=sw_emu
```

**消耗：**
- Kernel object（`.xo`）
- `config/<target>/link.cfg` — 连接规则（compute unit、内存绑定、时钟）
- `config/<target>/anvil.mk` 里的 platform `.xpfm`

**产物：**
```text
build/<preset>/src/kernels/<name>_xclbin/<name>.xclbin
build/<preset>/src/kernels/<name>_xclbin/.link.stamp
```

每个 mode 产生不同的 xclbin。`sw_emu`、`hw_emu`、`hw` 的 xclbin 不能互换。

这一步对硬件构建来说往往很慢。先跑 `csynth` 和 `cosim`，不要等 link 完才发现 kernel bug。

### `make analyze-link TARGET=<target> HOST_APP=<app>`

读 Vitis link 产物，展示 xclbin 里实际有什么。

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

**显示内容：**
- Xclbin 文件路径和大小
- Compute unit 名（如 `saxpy_1`、`vadd_1`）
- 内存端口绑定（如 `saxpy_1.x → DDR[0]`）
- 请求和实际达到的时钟设置
- Vitis link warning 或 error

调试 host app 之前先用这条命令。Xclbin 里 compute unit 或内存绑定错了，host app 没法在运行时修复。

### `make emconfig TARGET=<target> MODE=<mode>`

生成 XRT 在 `sw_emu` 和 `hw_emu` 运行时需要的 emulation 配置文件。

```bash
make emconfig TARGET=u250 MODE=sw_emu
make emconfig TARGET=u250 MODE=hw_emu  # 同一 target 生成同一个文件
```

**产物：**
```text
build/<target>/emconfig/emconfig.json
```

同一 target 的 `sw_emu` 和 `hw_emu` 共用一个 `emconfig.json`，每个 target 只需生成一次。在 `swemu` 或 `hwemu` 之前运行。

`make swemu` 和 `make hwemu` 依赖这个文件，如果不存在会自动构建。

---

## 构建 targets

### `make build TARGET=<target> HOST_APP=<app>`

构建一个 host 可执行文件。

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

**消耗：**
- Host 源文件：`src/host/<HOST_APP>.cpp`
- `anvil.mk` 里 `ANVIL_XRT_LIB` 提供的 XRT 头文件和库
- `src/kernels/include/kernels/` 里的 kernel ABI 头文件

**产物：**
```text
build/<preset>/src/host/<HOST_APP>
```

不综合 kernel，不创建 Python 环境。速度快。

嵌入式 target 交叉编译：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
```

---

## Run targets

这些 target 要求先构建好所有依赖（xclbin、host binary、dataset）。

### `make swemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

在 XRT software emulation 下运行 host app。

```bash
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**做什么：**
- 如果没有构建过就构建 `sw_emu` xclbin
- 确认 `emconfig.json` 存在
- 设置 `XCL_EMULATION_MODE=sw_emu` 和 `EMCONFIG_PATH=build/<target>/emconfig`
- 运行 host app

Software emulation 用 kernel 的快速行为 C 模型替代 FPGA 真实硬件，不跑 RTL 仿真。适合在不等硬件综合的情况下检查 host app 正确性。

**运行输出：** `runs/<target>/sw_emu/<host_app>/<dataset>/<run_key>/`

### `make hwemu TARGET=<target> HOST_APP=<app> DATASET=<data>`

在 XRT hardware emulation 下运行 host app。

```bash
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**做什么：**
- 用综合后的 kernel RTL 构建 `hw_emu` xclbin
- 确认 `emconfig.json` 存在
- 设置 `XCL_EMULATION_MODE=hw_emu` 和 `EMCONFIG_PATH=build/<target>/emconfig`
- 对 RTL 仿真跑 host app

Hardware emulation 比 software emulation 慢，因为跑的是真实 RTL 仿真。用非常小的数据集。能抓住 software emulation 漏掉的 RTL 正确性问题。

**要求：** 必须先跑 `csynth`。

**运行输出：** `runs/<target>/hw_emu/<host_app>/<dataset>/<run_key>/`

### `make hw TARGET=<target> HOST_APP=<app> DATASET=<data>`

在真实 FPGA 加速卡上运行 host app。

```bash
. /opt/xilinx/xrt/setup.sh
make hw TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

**要求：** XRT 已 source，`xbutil examine` 能看到加速卡，hardware xclbin 已构建。

**运行输出：** `runs/<target>/hw/<host_app>/<dataset>/<run_key>/`

### `make qemu TARGET=<embedded-target> HOST_APP=<app> DATASET=<data> QEMU_LAUNCHER=<script>`

在 QEMU 下跑嵌入式 target。

```bash
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny QEMU_LAUNCHER=/path/to/qemu-launch.sh
```

QEMU 和加速卡的 `hw_emu` 不是一回事。它模拟 ZynqMP 板卡的 ARM 处理器侧，需要 platform/image 特定的 launcher 脚本。Launcher 收到这些环境变量：`HOST_BIN`、`XCLBIN_PATH`、`DATA_DIR`、`RUN_DIR`、`OUTPUT`、`EMCONFIG_PATH`、`TARGET`、`HOST_APP`、`DATASET`、`ANVIL_PLATFORM`。它必须创建 `$OUTPUT`。

**运行输出：** `runs/<target>/qemu/<host_app>/<dataset>/<run_key>/`

---

## 正确性 targets

### `make compare DATASET=<name>`

把最近一次运行输出和 gold reference 对比。

```bash
make compare DATASET=tiny
```

读取 `data/<dataset>/gold_out.bin` 和最近一次运行的输出。打印通过/失败和误差统计。

### `make compare-hls KERNEL=<kernel> DATASET=<name>`

把 HLS model 输出和 gold 对比。

```bash
make compare-hls KERNEL=saxpy DATASET=tiny
```

在 `make test` 后用，当你想数值验证 HLS model 和 gold reference 结果一致时。

---

## Python 环境 target

### `make python-env`

创建项目 Python 虚拟环境。

```bash
make python-env
```

如果有 `uv` 就用 `uv`，否则 `python3 -m venv`。安装测试依赖。`make analyze`、`make analyze-cosim`、`make analyze-link` 和 Python 测试都需要它。

关闭默认镜像：

```bash
make python-env PYPI_INDEX=
```

从头重建：

```bash
make rebuild-python
```

---

## 部署 targets

部署到远程嵌入式板卡，见 [部署指南](deploy.md)。

```bash
make deploy-bin    TARGET=zcu102 HOST_APP=run_saxpy BOARD_IP=192.168.1.10
make deploy-xclbin TARGET=zcu102 BOARD_IP=192.168.1.10
make deploy-data   TARGET=zcu102 DATASET=tiny BOARD_IP=192.168.1.10
make deploy-check  TARGET=zcu102 BOARD_IP=192.168.1.10
make test-hw       TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny BOARD_IP=192.168.1.10
```

---

## 决策表：我应该跑哪条命令？

| 情况 | 命令 |
|---|---|
| 改了 C++ 逻辑或 Python 工具 | `make test` |
| 改了 kernel HLS 代码 | `make csynth TARGET=<t> KERNEL=<k>`，然后 `make analyze` |
| 想验证 RTL 和 C++ kernel 一致 | `make cosim TARGET=<t> KERNEL=<k>` |
| 改了 `link.cfg` | `make xclbin TARGET=<t>` |
| 改了 host app | `make build TARGET=<t> HOST_APP=<app>` |
| 改了数据格式 | `make gen` 和 `make gold` |
| 想快速无卡冒烟测 | `make swemu TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| 想 RTL 级无卡测试 | `make hwemu TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| 跑真实硬件 | `make hw TARGET=<t> HOST_APP=<app> DATASET=<data>` |
| 看报告数字 | `make analyze`、`make analyze-cosim`、`make analyze-link` |
| 配置 Python 工具 | `make python-env` |

---

## 常见构建失败

### `vitis_hls: command not found` 或 `v++: command not found`

Vitis 没有加入 PATH：

```bash
. /tools/Xilinx/Vitis/2024.2/settings64.sh
```

### platform `.xpfm` 文件找不到

`config/<target>/anvil.mk` 里 `ANVIL_PLATFORM` 路径不存在。安装 platform 或修正路径。

### Host build 找不到 XRT 头文件

XRT 没安装或 `anvil.mk` 里 `ANVIL_XRT_LIB` 路径错。默认通常是 `/opt/xilinx/xrt`。Source XRT：

```bash
. /opt/xilinx/xrt/setup.sh
```

### 嵌入式交叉编译失败：`crtbeginS.o` 或 `-lgcc` 找不到

Sysroot 和编译器不匹配。用和板卡 image 及 Vitis 版本匹配的 PetaLinux sysroot。

### Cosim target 不存在

Kernel 在 `CMakeLists.txt` 里注册时没有 `TESTBENCH` 参数，或 `anvil.mk` 里 `ANVIL_COSIM_TARGETS` 没包含它。

### `make xclbin` 报 kernel not found

`link.cfg` 里的 `nk=<name>` 和综合后的 kernel 顶层函数名不匹配。确认 `nk=saxpy:1:saxpy_1` 对应 `extern "C" void saxpy(...)`。
