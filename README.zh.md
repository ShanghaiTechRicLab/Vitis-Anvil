# Vitis-Anvil

**A CMake project template for forging Vitis/XRT FPGA accelerators.**

Anvil 是铁砧：硬件在上面被锻造成型。Vitis-Anvil 提供一个小而完整、可复现的 FPGA 加速器工程模板：CMake presets、Vitis HLS kernel、XRT host 程序、CPU gold reference、数据集、HLS/cosim 分析、板端部署辅助脚本，以及按设备拆分的配置。

项目名故意短、适合 CLI：未来的 `anvil init`、`anvil build` 这类薄封装可以自然映射到本文档里的 Make/CMake 流程。

## 从这里开始

- [文档索引](docs/zh/index.md)
- [快速开始：跑完整流程](docs/zh/get_started.md)
- [加速卡流程：U250/U50/U55C/U200/U280/VCK5000](docs/zh/accelerator_flow.md)
- [Embedded 流程：ZCU102/ZCU104/ZCU106/KV260](docs/zh/embedded_flow.md)
- [自定义 kernel、host app、设备、数据集和报告](docs/zh/customization.md)
- [把 Vitis-Anvil 适配到你自己的项目](docs/zh/adapt_to_your_project.md)
- [典型开发流程](docs/zh/development_workflow.md)

## 包含什么

| 领域 | 内容 |
|---|---|
| 构建系统 | CMake presets、`config/<target>/anvil.mk`、顶层 Make targets |
| Kernels | `saxpy`、`vadd`、加速卡 stream pipeline demos |
| Host apps | `run_saxpy`、`run_vadd`、`run_pipeline_demo` |
| 验证 | CPU 测试、gold 生成、结果比较、HLS csynth、HLS cosim、XRT 运行路径 |
| 分析 | `hlsflow` rich 终端报告、HTML/TXT 导出、JSONL run database |
| 目标设备 | Alveo 与 embedded Zynq/ZynqMP 类 presets，platform 路径可覆盖 |

## 常用命令速查

```bash
make test                                      # CPU-only 快速测试；不需要 Vitis/XRT/platform
make build TARGET=u250 HOST_APP=run_saxpy     # 构建选定 host app
make csynth TARGET=u250 KERNEL=saxpy          # HLS 综合
make cosim TARGET=u250 KERNEL=saxpy           # HLS C/RTL cosim
make analyze-flow TARGET=u250 KERNEL=saxpy    # csynth 报告分析
make analyze-cosim TARGET=u250 KERNEL=saxpy   # cosim 报告分析
make xclbin TARGET=u250                       # 硬件 xclbin 链接；耗时较长
make run-host TARGET=u250 HOST_APP=run_saxpy  # 在已安装的加速卡上运行
```

Embedded 板卡使用 `PETALINUX_SYSROOT=... make build-host TARGET=zcu102`，完整部署流程见 [docs/zh/embedded_flow.md](docs/zh/embedded_flow.md)。

## License

见 [LICENSE](LICENSE)。Vendored third-party code 保留其上游 license，位于 `third_party/`，汇总见 `third_party/THIRD_PARTY_NOTICES.md`。
