# 构建系统模型

Vitis-Anvil 把 FPGA 流程当成分阶段 artifact graph，而不是一组 shell script。

核心规则是：

```text
用户命令 -> 真实 artifact stamp -> stage-specific action key -> manifest
```

`csynth`、`xclbin`、`hwemu` 这类 Make target 可以是 phony，但它们只能依赖真实文件。耗时的 Vitis 工作由 CMake/Ninja 规则拥有，并声明 stamp、byproducts 和 depfile。

## 三层结构

### 1. 用户命令层

```bash
make csynth TARGET=u250 KERNEL=saxpy
make xclbin TARGET=u250 HOST_APP=run_saxpy
make hwemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make swemu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make qemu TARGET=zcu102 HOST_APP=run_saxpy DATASET=tiny
make analyze TARGET=u250 KERNEL=saxpy
```

这些命令表达意图，不能隐藏无条件 Vitis 重跑。

### 2. artifact action 层

重活按这种形态建模：

```cmake
add_custom_command(
  OUTPUT      <stage>.stamp
  BYPRODUCTS real.outputs action.json action.sha256 env.json env.sha256 manifest.json
  DEPENDS     declared.inputs
  DEPFILE     deps.d
  COMMAND     python -m hlsflow.action_runner ... -- actual-tool ...
)
```

只有命令成功后才 touch stamp。Vitis 中途失败时，stamp 不更新。
现在 `action.sha256` 是和 stamp 并列写出的解释性元数据；Ninja 不能用同一条
command 产生的文件来决定这条 command 是否要重跑。必须让本地构建失效的输入，
仍然要出现在 `DEPENDS`、`DEPFILE` 或 custom command line 中。该元数据是后续
预计算 action-cache/CAS 层的接口。

### 3. 缓存元数据层

每个 action 写出：

| 文件 | 含义 |
|---|---|
| `action.json` | 规范化后的 stage 输入、命令、配置和输出名 |
| `action.sha256` | 诊断/manifest 使用的 policy hash；未来 CAS key |
| `env.json` | 诊断用工具/环境信息 |
| `env.sha256` | 默认只是软警告；strict 模式下可参与失效 |
| `manifest.json` | 实际产物、大小和 action hash |

本地增量仍由 Ninja 处理；这些元数据让每条边可解释，并为后续 CAS/远程缓存留接口。

## stage-specific key

不要把 `MODE` 无脑放进所有 key。每个 stage 自己定义什么会影响输出。

| Stage | MODE 是否进 key | 说明 |
|---|---:|---|
| `data` | 否 | 数据集参数、seed、dtype、generator 决定输出 |
| `gold` | 否 | data digest 和 gold 逻辑决定输出 |
| `hls-model` | 否 | CPU-only；不能依赖 Vitis/XRT/platform/board |
| `host` | 通常否 | accelerator host 在 ABI 一致时跨 `sw_emu/hw_emu/hw` 共享 |
| `csynth` | 只有真实编译配置不同时 | `.xo` 默认不按 mode 三份缓存 |
| `cosim` | 只有 simulator 配置不同时 | testbench/data 变只重跑 cosim，不重跑 csynth |
| `xclbin` | 是 | `sw_emu/hw_emu/hw` 的 xclbin 不同 |
| `emconfig` | sw/hw emu 之间否 | target 级 `emconfig.json` 共享 |
| `run` | 是 | 输出按 target/mode/host/dataset/run key 隔离 |
| `qemu` | 仅 embedded | 准备 AArch64 host、embedded emulation xclbin 和 emconfig；真正 QEMU 启动取决于 platform/image |
| `analyze` | 否 | 默认只读已有报告；`BUILD=1` 才允许构建缺失报告 |
| `deploy` | 是 | bundle 可缓存；SSH copy 是 side effect |

## 关键路径

```text
build/<preset>/src/kernels/<kernel>_hls/.csynth.stamp
build/<preset>/src/kernels/<kernel>_hls/<kernel>.xo
build/<preset>/src/kernels/<kernel>_hls/.csynth.d
build/<preset>/src/kernels/<kernel>_hls/csynth.manifest.json

build/<preset>/src/kernels/<xclbin>_xclbin/.link.stamp
build/<preset>/src/kernels/<xclbin>_xclbin/<xclbin>.xclbin
build/<preset>/src/kernels/<xclbin>_xclbin/link.manifest.json

build/<target>/emconfig/emconfig.json
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/out.bin
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/run.json
runs/<target>/<mode>/<host_app>/<dataset>/<run_key>/stdout.log
```

`data/` 只放输入数据。运行输出不写回 `data/`。

## header dependency

HLS C++ header 依赖用 depfile 跟踪，最终模型不 glob 整个 include 目录。

依赖扫描使用 compiler 风格参数：

```text
-MMD -MP -MF .csynth.d -MT .csynth.stamp
```

生成头必须先存在；缺失 header 应该失败，而不是被掩盖。

## analyze 行为

`make analyze` 默认只读已有报告，不应该意外启动小时级 Vitis 构建。

只解析已有报告：

```bash
make analyze TARGET=u250 KERNEL=saxpy
```

明确允许先构建缺失报告：

```bash
make analyze TARGET=u250 KERNEL=saxpy BUILD=1
```

## 为什么重要

FPGA 构建很贵。构建图里每条边都必须回答四个问题：

1. 为什么要重跑？
2. 为什么不重跑是安全的？
3. 输出在哪里？
4. 失败后 stamp 有没有被污染？

回答不了这些问题的规则，还没有真正构建系统化。

## 这个设计参考的外部模型

- CMake `add_custom_command(OUTPUT ... BYPRODUCTS ... DEPFILE ...)` 是告诉
  Ninja “这条 rule 拥有哪些文件”的基础原语。
- Ninja depfile 是 C/C++ header 发现的正确模型；不要用手写目录 glob 代替。
- Bazel 的 action-cache/CAS 模型启发了 `action.json` + `manifest.json` 元数据；
  当前实现仍然依赖本地 Ninja timestamp 做失效判断。
- AMD `emconfigutil` 支持同一个 `emconfig.json` 用于 software/hardware
  emulation，所以 Anvil 把它放在 `build/<target>/emconfig/`。
- 公开的 CMake/Vitis-HLS 项目大多只是包装 Vitis 命令；Anvil 额外要求每个
  stage 都有可解释的失效规则、stamp、byproducts、depfile 和 manifest。
