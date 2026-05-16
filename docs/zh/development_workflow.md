# 开发流程

这篇给日常改代码的顺序。核心规则：先用最便宜的检查抓 bug，再用更慢的 FPGA 步骤。

## 1. 调试阶梯

按这个顺序：

1. **CPU unit tests** — 抓普通 C++/Python 错误。
2. **Gold vs HLS model** — 在 Vitis 前抓数据布局错误。
3. **HLS 综合 (`csynth`)** — 抓 HLS 不兼容 C++，查看硬件估算。
4. **HLS cosim (`cosim`)** — 抓 RTL 行为不一致。
5. **xclbin link** — 抓 platform/connectivity/memory-bank 问题。
6. **host build** — 抓 XRT/API/编译问题。
7. **硬件运行** — 抓 runtime、BO group、设备、部署问题。
8. **compare/analyze** — 检查正确性和性能趋势。

不要改完 kernel 直接跑硬件。等待更久，错误还更不精确。

## 2. 改普通 C++ 或 Python 代码

运行：

```bash
make test
```

如果涉及 Python 工具：

```bash
make python-env
make test
```

这应该很快，不需要 Vitis 或 XRT。

## 3. 改 kernel

运行：

```bash
make csynth TARGET=u250 KERNEL=<kernel>
make analyze-flow TARGET=u250 KERNEL=<kernel>
make cosim TARGET=u250 KERNEL=<kernel>
make analyze-cosim TARGET=u250 KERNEL=<kernel>
```

怎么理解：

- csynth 编译失败：kernel 代码或 include path 问题
- csynth II/timing 差：HLS 结构问题
- cosim 失败：算法/RTL 不一致或 testbench 问题
- analyze 输出异常：parser 或 report 路径问题

## 4. 改 host 代码

运行：

```bash
make build TARGET=u250 HOST_APP=<app>
```

这只构建 host app，不应该综合 kernel。

如果 host app 编译通过但运行失败，检查：

- XRT setup
- xclbin 路径
- compute-unit 名
- BO group index
- dataset 路径

## 5. 改 `link.cfg`

运行：

```bash
make xclbin TARGET=u250
```

`link.cfg` 改动不是 C++ unit test 能测的，要靠 Vitis linker 和 host runtime。

## 6. 加板卡

先 configure/synthesis，再硬件：

```bash
make csynth TARGET=<target> KERNEL=saxpy
make analyze-flow TARGET=<target> KERNEL=saxpy
```

之后再试：

```bash
make xclbin TARGET=<target>
make build TARGET=<target> HOST_APP=run_saxpy
```

Embedded 板卡构建 host 时要设置 `PETALINUX_SYSROOT`。

## 7. 提交策略

好的提交应该小而按层分：

1. gold/reference + tests
2. kernel ABI/header
3. kernel 实现 + cosim
4. CMake/Make 注册
5. host app
6. board config/link.cfg
7. docs

不要把“新 kernel 实现”和“新板卡配置”混在一个提交里，除非它们不可分。

## 8. 最终说明记录什么

较大改动最后记录：

- 跑过哪些命令
- 测过哪些 target
- 是否跑过 Vitis hardware link
- 是否跑过真实硬件
- 哪些分支没测，例如 older XRT fallback 或 embedded sysroot 路径
