# 加速卡流程

本文面向 PCIe/XRT 加速卡，例如 U250、U50、U55C、U200、U280、VCK5000。

## 1. 安装并发现 platform

加速卡流程需要三件东西：

1. Vitis tools：`v++`、`vitis-run`。
2. XRT headers/libraries/runtime。
3. 和板卡 shell 匹配的 platform `.xpfm`。

查找 platform 文件：

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

如果 `config/<target>/anvil.mk` 里的默认路径不对，用命令行覆盖：

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

路径稳定后，也可以直接改 `config/u50/anvil.mk`。

## 2. 构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

Host app 选择：

| `HOST_APP=` | 用途 |
|---|---|
| `run_saxpy` | saxpy demo host |
| `run_vadd` | vadd demo host |
| `run_pipeline_demo` | U250 stream pipeline demo |

`HOST_APP=run_pipeline_demo` 当前要求 `TARGET=u250`。

## 3. Synthesize 和 cosim kernel

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

`KERNEL=vadd` 跑 vadd，`KERNEL=all` 跑该 target 配置的默认 kernel 集合。Stream pipeline 使用：

```bash
make csynth-stream TARGET=u250
make cosim-stream TARGET=u250
```

## 4. Link 并运行 xclbin

```bash
make xclbin TARGET=u250
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

硬件仿真：

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## 5. U250 pipeline demo

```bash
make pipeline-demo TARGET=u250
make run-host TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny
```

Pipeline demo 构建独立的 `pipeline_demo.xclbin`；当 `HOST_APP=run_pipeline_demo` 时 Makefile 会自动选择它。

## 6. 加速卡健康检查

调试代码前，先确认板卡和 XRT 正常：

```bash
xbutil examine
xbutil validate -d 0
xbutil --version
```

常见问题：

| 现象 | 可能原因 | 修复 |
|---|---|---|
| `.xpfm missing` | platform package 未安装或路径不同 | 用 `find ... -name '*.xpfm'` 找到路径并设置 `ANVIL_PLATFORM=` |
| `Could NOT find XRT` | XRT headers/libraries 未安装或不在 `/opt/xilinx/xrt` | 安装/source XRT，或更新 `cmake/FindXRT.cmake` hints |
| xclbin load 失败 | xclbin 和当前 shell/platform 不匹配 | 用完全匹配的 platform 重建 |
| host app 能跑但 compare 失败 | dataset/gold/host output contract 不一致 | 检查 `scripts/gen_dataset.py`、gold、host output path |
