# 把 Vitis-Anvil 适配到你的项目

这篇讲如何把模板变成真实加速器仓库。如果你还没加过 kernel，先读 [自定义指南](customization.md)。

## 1. 决定保留什么

多数项目会保留：

- 顶层 Makefile 流程
- CMake presets 和 helper modules
- `include/anvil/**` 框架辅助
- `src/anvil/**` runtime/logging 库
- `tools/hlsflow/**` 报告工具
- docs 结构

多数项目会替换：

- `src/kernels/` 里的 demo kernels
- `src/host/` 里的 demo host apps
- `src/gold/` 里的 demo gold reference
- demo dataset 和 compare 逻辑
- `config/` 下的板卡配置

## 2. 先换问题，不要先换框架

不要一开始就重命名所有 `anvil` namespace。先保持框架稳定，在它周围加你的项目代码。

好的第一步：

```text
src/kernels/include/kernels/my_algorithm.hpp
src/kernels/my_algorithm_kernel.cpp
src/host/run_my_algorithm.cpp
src/gold/include/gold/my_algorithm_gold.hpp
```

不好的第一步：

```text
rename include/anvil to include/my_company
还没跑通 kernel 就重写 runtime wrappers
```

太早重命名框架会制造大量错误，但不会让硬件更快跑起来。

## 3. 分层替换 demo

推荐顺序：

1. 保持 `saxpy` 能跑。
2. 把你的新 kernel 加在旁边。
3. 把你的 host app 加在现有 host app 旁边。
4. 加你的 dataset/gold/compare 流程。
5. 加你的板卡配置。
6. 你的流程跑通后再删除 demo。

这样调试时始终有一个已知正确的参考。

## 4. 定义项目契约

写清楚：

- 输入文件
- 输出文件
- metadata JSON 字段
- kernel 参数顺序
- pack 宽度
- 目标板卡
- 误差阈值
- 性能检查标准

项目变大前就把这些写进文档。很多 FPGA bug 实际上是 host、kernel、data tools 之间的契约不一致。

## 5. 逐步添加 CI

有用的 CI 阶梯：

1. 格式/静态检查（如果有）
2. `make test`
3. Python tests
4. install smoke
5. 如果 runner 有 Vitis，可选对一个小 kernel 跑 csynth
6. 硬件测试只放在专门机器上

不要让每个 PR 都跑完整 hardware link，除非你有足够机器和时间。
