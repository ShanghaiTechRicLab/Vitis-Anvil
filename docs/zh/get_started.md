# 快速开始：把流程完整跑一遍

这篇按 FPGA 开发的正常顺序带你跑一遍项目。目标不是追求性能，而是理解每一步在做什么，并确认你的环境能跑。

如果这些词还不熟：kernel、host app、xclbin、XRT、csynth、cosim，先读 [基本概念](concepts.md)。

## 1. 你会跑哪些层级

| 层级 | 命令 | 需要 Vitis? | 需要 XRT? | 需要 FPGA 硬件? | 目的 |
|---|---|---:|---:|---:|---|
| CPU-only 测试 | `make test` | 否 | 否 | 否 | 检查普通 C++/Python 代码和示例 |
| HLS 综合 | `make csynth TARGET=... KERNEL=...` | 是 | 否 | 否 | 检查 Vitis 能否把 kernel C++ 转成硬件 |
| HLS cosim | `make cosim TARGET=... KERNEL=...` | 是 | 否 | 否 | 检查生成的 RTL 是否和 C++ kernel 行为一致 |
| 硬件运行 | `make xclbin`、`make run-host` 或 deploy | 是 | 是 | 是或 hw_emu | 构建/加载 FPGA 二进制并执行 |

不要一上来就跑硬件。先 CPU-only，再 synthesis，再 cosim，再 xclbin/host。

## 2. 检查仓库状态

在仓库根目录运行：

```bash
git status --short
```

工作树干净时最容易调试。构建输出在 `build/`，生成数据在 `data/`，报告在 `reports/`。

## 3. 跑 CPU-only 测试

```bash
make test
```

这个命令做什么：

1. 配置 `hls-model-linux-debug` preset。
2. 构建 CPU 侧代码：gold reference、HLS model、CLI 工具、测试。
3. 运行 CTest。

它**不会**做什么：

- 不跑 Vitis 综合
- 不链接 xclbin
- 不访问 XRT 设备
- 不运行 FPGA 硬件

期望结果：

```text
100% tests passed
```

如果这里失败，先修这里。CPU-only 失败通常是普通 C++/Python 或数据格式问题。

## 4. 认识 demo 的名字

默认 demo 有这些 kernel：

| Kernel | 计算什么 | 文件位置 |
|---|---|---|
| `saxpy` | `out = a*x + y` | `src/kernels/saxpy_kernel.cpp` |
| `vadd` | `out = a + b` | `src/kernels/vadd_kernel.cpp` |
| `pipeline_demo` | `saxpy_stream -> vadd_stream` | `src/kernels/` 里的 stream kernel |

默认 host app：

| Host app | 运行什么 |
|---|---|
| `run_saxpy` | 加载包含 `saxpy_1` 的 xclbin |
| `run_vadd` | 加载包含 `vadd_1` 的 xclbin |
| `run_pipeline_demo` | 加载 stream pipeline xclbin |

## 5. 跑 HLS 综合

选择一个 target。如果你安装了 Vitis 2024.2 且 platform 存在，U250 是常见加速卡 target：

```bash
make csynth TARGET=u250 KERNEL=saxpy
```

这个命令做什么：

1. 读取 `config/u250/anvil.mk`，找到 Vitis platform 和 part。
2. 配置该 target 的 CMake preset。
3. 对 `saxpy` kernel 运行 Vitis HLS compile。
4. 在 `build/<preset>/src/kernels/` 下写 HLS 工作目录和报告。

重要产物是 csynth XML 报告，路径通常类似：

```text
build/u250-host/src/kernels/saxpy_hls/hls/syn/report/saxpy_csynth.xml
```

然后查看报告：

```bash
make analyze-flow TARGET=u250 KERNEL=saxpy
```

报告会告诉你 timing、II、latency、resource。此时还没有在 FPGA 上运行。

## 6. 跑 HLS 协同仿真

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

这个命令做什么：

1. 构建或复用 HLS kernel 输出。
2. 用 `tests/kernels/` 下的 C++ testbench 跑生成的 RTL。
3. 生成 cosim 报告。
4. 提取 pass/fail 和 latency。

如果 cosim 失败，不要先去查 XRT 或 host。问题在 kernel 或 testbench。

## 7. 链接 xclbin

```bash
make xclbin TARGET=u250
```

这个命令做什么：

1. 使用综合得到的 kernel object，例如 `saxpy_xo`、`vadd_xo`。
2. 读取 `config/u250/link.cfg`，拿到 compute-unit 名和内存 bank 绑定。
3. 调用 Vitis linker。
4. 在 build tree 下生成 xclbin。

典型输出路径：

```text
build/u250-host/src/kernels/saxpy_xclbin/saxpy.xclbin
```

link 完以后，查看 Vitis 实际生成了什么：

```bash
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

它会解析 link 产物，显示 xclbin 路径、compute unit、来自 `link.cfg` 的内存 bank 绑定、clock 设置，以及 Vitis link warning/error。它回答的是：xclbin 有没有生成，`saxpy_1` 有没有进 xclbin，端口是不是绑到了预期的 DDR/HBM bank。

这一步可能很慢。硬件构建比 CPU 测试慢很多是正常的。

## 8. 构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
```

这个命令做什么：

1. 配置 host CMake preset。
2. 只构建选中的 host 可执行文件。
3. 不跑综合，也不创建 Python 环境。

host binary 通常在：

```text
build/u250-host/src/host/run_saxpy
```

## 9. 生成输入和 gold 输出

```bash
make gen DATASET=tiny
make gold DATASET=tiny
```

这个命令做什么：

- `make gen` 在 `data/tiny/` 下创建输入文件和 `meta.json`。
- `make gold` 运行 CPU reference，写入期望输出。

之后 FPGA 运行也应该把硬件输出写到同一个 dataset 目录，方便 compare 检查。

## 10. 在硬件或硬件仿真上运行

如果机器上有支持的 FPGA 卡，并且 XRT 已 source：

```bash
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

这个命令会用 xclbin 路径和 dataset 路径调用 host app。host app 会加载 xclbin、拷贝输入 buffer、启动 kernel、读回输出、写结果文件。

然后比较：

```bash
make compare DATASET=tiny
```

如果你使用 hardware emulation，读 [加速卡流程](accelerator_flow.md) 的对应说明。

## 11. 嵌入式板卡

嵌入式板卡多两个概念：

- host app 交叉编译需要 sysroot
- 板卡文件系统/镜像里要有 XRT

例子：

```bash
PETALINUX_SYSROOT=/opt/Xilinx/images/xilinx-zynqmp-common-v2024.2/sysroots/cortexa72-cortexa53-xilinx-linux \
  make build-host TARGET=zcu102 HOST_APP=run_saxpy

make csynth TARGET=zcu102 KERNEL=saxpy
make xclbin TARGET=zcu102
```

然后把文件部署到板上。见 [Embedded 流程](embedded_flow.md) 和 [部署指南](deploy.md)。

## 12. 怎样算成功

第一次跑通应该满足：

- `make test` 通过。
- 至少一个 kernel 通过 `csynth`。
- 同一个 kernel 通过 `cosim`。
- host app 可以构建。
- 如果有硬件，硬件输出和 gold 输出一致。

之后再读 [自定义指南](customization.md)，把 demo 换成你自己的算法。
