# 典型开发流程

核心思路：把快速的 CPU 迭代和慢速的 FPGA 工作拆开。你不会想因为修了 host 文件里的一个 typo 就等一次 HLS 综合，也不会想通过 Vitis 日志来调试 host 逻辑。

## 日常循环

```bash
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

普通 C++ 或 Python 改动后跑这个。它不会自动创建 Python 环境（除非你显式调用 `make python-env`），也不会触发 kernel 综合。只是编译 host 程序并跑 CPU 侧的测试。

## Kernel 循环

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

改 HLS 代码或 pragmas 的时候用。重点看：

- II（initiation interval）
- 延迟（latency）
- Timing slack
- 资源余量（LUT、DSP、BRAM）
- Interface 摘要

## Cosim 循环

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosimulation 用 kernel testbench 验证 Vitis 从你的 C++ 代码生成的 RTL。这一步能在你构建 xclbin 之前抓住 ABI 不匹配和接口问题。它不运行 XRT host 程序。

## XRT host 循环

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make xclbin TARGET=u250
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

改了 kernel ABI 或 host 端的 buffer 逻辑后跑这个。

## Embedded board 循环

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## 提交前检查

```bash
make test
make analyze-flow TARGET=<target> KERNEL=<kernel>
make analyze-cosim TARGET=<target> KERNEL=<kernel>
```

如果改动涉及硬件，还应跑 `make run-host` 或 board deployment 路径。

## Debug 顺序

1. `make test` 失败：先修 CPU 端逻辑、库问题或 golden reference。
2. `make csynth` 失败：检查 HLS 编译日志和 C++14/HLS 限制。
3. `make cosim` 失败：检查 kernel testbench 和 ABI 假设。
4. `make xclbin` 失败：检查 `link.cfg`、platform、memory banks、clock 约束。
5. Host run 失败：检查 XRT device 选择、xclbin 路径、CU 名称、buffer group ID。
6. Compare 失败：检查 dataset、golden reference 和 host 输出路径 — 它们的数据格式必须一致。
