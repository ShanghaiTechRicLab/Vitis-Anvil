# Vitis-Anvil 文档

Vitis-Anvil 是一个用于构建 Vitis/XRT FPGA 加速器的 CMake 项目模板。文档默认你只知道 FPGA/HLS 的大概概念；每一步都会先解释目的，再给命令。

建议阅读顺序：

1. [基本概念](concepts.md) — kernel、host app、xclbin、XRT、csynth、cosim、TARGET/KERNEL/HOST_APP 分别是什么。
2. [快速开始](get_started.md) — 从 CPU-only 测试一路跑到可用于硬件的产物。
3. [项目结构](project_structure.md) — 每一层代码放在哪里，为什么要分开。
4. [构建指南](build.md) — 常用 Make target 做什么、生成什么、什么时候用。
5. [加速卡流程](accelerator_flow.md) — U250/U50/U55C/U200/U280/VCK5000 这类 PCIe 加速卡的流程。
6. [Embedded 流程](embedded_flow.md) — ZCU102/ZCU104/ZCU106/KV260 这类板卡的流程。
7. [部署指南](deploy.md) — 文件如何放到真实板卡/加速卡上运行，出错怎么查。
8. [自定义指南](customization.md) — 添加自己的 kernel、host app、数据集、板卡和报告。
9. [适配到自己的项目](adapt_to_your_project.md) — 把模板变成真实产品仓库。
10. [开发流程](development_workflow.md) — 日常迭代顺序和调试顺序。
11. [hlslib 适配](hlslib_adaptation.md) — pack/stream/dataflow 辅助，以及框架代码和用户代码的边界。
12. [用户手册](user-guide.md) — 命令速查和参考。

如果读着读着不知道某个词是什么意思，回到 [基本概念](concepts.md)。大多数混乱来自把 kernel 级步骤（`csynth`、`cosim`、`xclbin`）和 host 级步骤（`build-host`、`run-host`、deploy）混在一起。
