# 加速卡流程

加速卡（U250、U50、U55C、U200、U280、VCK5000）通过 PCIe 连接，用 XRT 做运行时管理。不管用哪张卡，流程都一样 — 你只需要改 `TARGET` 变量。

## 1. 安装并查找 platform

加速卡流程需要三样东西：

1. Vitis 工具（`v++`、`vitis-run` 等）
2. XRT 头文件、库和运行时
3. 和卡及 shell 版本匹配的 platform `.xpfm` 文件

查看本机有哪些 platform 文件：

```bash
find /opt /tools/Xilinx -name '*.xpfm' 2>/dev/null
```

如果 `config/<target>/anvil.mk` 里的默认路径和你的安装位置对不上，可以在命令行覆盖：

```bash
make build TARGET=u50 ANVIL_PLATFORM=/path/to/xilinx_u50_....xpfm
```

路径稳定后，也可以直接改 `config/u50/anvil.mk`，省得每次都传。

## 2. 构建 host app

```bash
make build TARGET=u250 HOST_APP=run_saxpy
make build TARGET=u250 HOST_APP=run_vadd
```

可用的 host app：

| `HOST_APP=` | 用途 |
|---|---|
| `run_saxpy` | saxpy demo host |
| `run_vadd` | vadd demo host |
| `run_pipeline_demo` | 加速卡 stream pipeline demo |

`run_pipeline_demo` 需要 `config/<target>/pipeline_demo.cfg`。仓库已经为 `u250`、`u55c`、`u50`、`u200`、`u280`、`vck5000` 提供了配置。

## 3. 综合和仿真 kernel

```bash
make csynth TARGET=u250 KERNEL=saxpy
make cosim TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

`KERNEL=vadd` 跑 vadd，`KERNEL=all` 跑该 target 配置的所有 kernel。Stream pipeline 有独立的 target：

```bash
make csynth-stream TARGET=u250   # 也支持 u55c、u50、u200、u280、vck5000
make cosim-stream TARGET=u250    # 也支持 u55c、u50、u200、u280、vck5000
```

## 4. 链接并运行 xclbin

```bash
make xclbin TARGET=u250
make gen DATASET=tiny
make gold DATASET=tiny
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

硬件仿真（不需要物理卡）：

```bash
make xclbin-hwemu TARGET=u250
make xrt-emu TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

## 5. U250 pipeline demo

```bash
make pipeline-demo TARGET=u250
make run-host TARGET=u250 HOST_APP=run_pipeline_demo DATASET=tiny

# u55c/u50/u200/u280/vck5000 安装了对应 platform 后也用同样的命令。
```

Pipeline demo 会构建一个独立的 `pipeline_demo.xclbin`。当 `HOST_APP=run_pipeline_demo` 时，Makefile 会自动选它。

## 6. 板卡健康检查

在调试自己的代码之前，先确认板卡和 XRT 工作正常：

```bash
xbutil examine
xbutil validate -d 0
xbutil --version
```

## 常见问题

| 现象 | 可能原因 | 修复 |
|---|---|---|
| `.xpfm missing` | Platform 包未安装或路径不同 | 用 `find ... -name '*.xpfm'` 找到路径并设置 `ANVIL_PLATFORM=` |
| `Could NOT find XRT` | XRT 未安装或不在 `/opt/xilinx/xrt` | 安装或 source XRT；必要时更新 `cmake/FindXRT.cmake` |
| xclbin 加载失败 | xclbin 用不同的 shell 版本链接 | 用和当前 shell 完全匹配的 platform 重建 |
| Host 能跑但 compare 失败 | dataset、gold、host 输出格式不一致 | 检查 `scripts/gen_dataset.py`、gold 代码和 host 输出路径 |
