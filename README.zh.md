# Vitis-Anvil

![Vitis-Anvil banner](docs/assets/vitis-anvil-banner.png)

**一个用来搭建 Vitis/XRT FPGA 加速器的 CMake 项目模板。**

Anvil 是铁砧 — 把金属放上去锻造成型。Vitis-Anvil 对 FPGA 开发做同样的事：提供一个项目骨架，把 CMake presets、Vitis HLS kernel、XRT host 程序、CPU golden reference、数据集、HLS/cosim 分析、板端部署脚本、按设备拆分的配置这些都包好，让你可以专心写加速器逻辑本身。

名字故意取短，因为要在命令行里反复敲。以后想做 `anvil init`、`anvil build`、`anvil csynth` 这类薄封装，直接映射到这里的 Makefile 和 CMake 目标就行。

## 从这里开始

- [文档索引](docs/zh/index.md) — 第一次用建议按顺序读
- [快速开始：跑完整流程](docs/zh/get_started.md) — 从 clone 跑到硬件
- [加速卡流程](docs/zh/accelerator_flow.md) — U250/U50/U55C/U200/U280/VCK5000
- [Embedded 流程](docs/zh/embedded_flow.md) — ZCU102/ZCU104/ZCU106/KV260
- [自定义 kernel、host app、设备、数据集和报告](docs/zh/customization.md)
- [把 Vitis-Anvil 适配到你自己的项目](docs/zh/adapt_to_your_project.md)
- [典型开发流程](docs/zh/development_workflow.md)
- [hlslib 适配模式](docs/zh/hlslib_adaptation.md)

## 包含什么

| 领域 | 内容 |
|---|---|
| 构建系统 | CMake presets、`config/<target>/anvil.mk`、顶层 Make targets |
| Kernels | `saxpy`、`vadd`、加速卡 stream pipeline demos |
| Host apps | `run_saxpy`、`run_vadd`、`run_pipeline_demo` |
| 验证 | CPU 测试、gold 生成、结果比较、HLS 综合、HLS 协同仿真、XRT 硬件运行路径 |
| 分析 | `hlsflow` 终端报告、HTML/TXT 导出、JSONL 运行数据库 |
| 目标设备 | Alveo 和 embedded Zynq/ZynqMP presets，platform 路径可覆盖 |

## 常用命令速查

```bash
make test                                      # 仅 CPU 测试（不需要 Vitis/XRT/platform）
make build TARGET=u250 HOST_APP=run_saxpy     # 构建一个 host app
make csynth TARGET=u250 KERNEL=saxpy          # HLS 综合
make cosim TARGET=u250 KERNEL=saxpy           # HLS C/RTL 协同仿真
make analyze-flow TARGET=u250 KERNEL=saxpy    # 查看综合报告
make analyze-cosim TARGET=u250 KERNEL=saxpy   # 查看协同仿真报告
make xclbin TARGET=u250                       # 链接硬件 xclbin（耗时较长）
make run-host TARGET=u250 HOST_APP=run_saxpy  # 在已安装的加速卡上运行
```

嵌入式板卡需要设置 `PETALINUX_SYSROOT`：

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102
```

完整部署流程见 [docs/zh/embedded_flow.md](docs/zh/embedded_flow.md)。

## License

见 [LICENSE](LICENSE)。第三方代码保留其上游 license，位于 `third_party/`，汇总见 `third_party/THIRD_PARTY_NOTICES.md`。
