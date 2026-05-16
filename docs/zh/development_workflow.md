# 典型开发流程

核心原则：把快速 CPU 迭代和慢速 FPGA 工作拆开。

## 日常循环

```bash
make test
make build TARGET=u250 HOST_APP=run_saxpy
```

普通 C++/Python 改动后跑这个。它不应该自动创建 Python 环境，除非你显式调用 `make python-env`；也不应该触发 kernel synthesis。

## Kernel 循环

```bash
make csynth TARGET=u250 KERNEL=saxpy
make analyze-flow TARGET=u250 KERNEL=saxpy
make check-hls
```

修改 HLS 代码或 pragmas 时使用。重点看：

- II
- latency
- timing slack
- resource headroom
- interface summary

## Cosim 循环

```bash
make cosim TARGET=u250 KERNEL=saxpy
make analyze-cosim TARGET=u250 KERNEL=saxpy
```

Cosim 用 kernel testbench 验证 HLS 生成的 RTL。它不运行 XRT host app。

## XRT host 循环

```bash
make gen DATASET=tiny
make gold DATASET=tiny
make xclbin TARGET=u250
make run-host TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
make compare DATASET=tiny
```

Kernel ABI 或 host buffer 改动后跑这个。

## Embedded board 循环

```bash
PETALINUX_SYSROOT=/path/to/sysroot make build-host TARGET=zcu102 HOST_APP=run_saxpy
make xclbin TARGET=zcu102
make deploy TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
make test-xrt-hw TARGET=zcu102 BOARD_IP=192.168.1.100 DATASET=tiny
```

## Commit 前建议检查

```bash
make test
make analyze-flow TARGET=<target> KERNEL=<kernel>
make analyze-cosim TARGET=<target> KERNEL=<kernel>
```

硬件相关改动还应跑对应 `run-host` 或 board deployment 路径。

## Debug 顺序

1. `make test` 失败：先修 CPU/library/gold 逻辑。
2. `make csynth` 失败：看 HLS compile logs 和 C++14/HLS 限制。
3. `make cosim` 失败：看 kernel testbench 和 ABI 假设。
4. `make xclbin` 失败：看 `link.cfg`、platform、memory banks、clock constraints。
5. host run 失败：看 XRT device、xclbin path、CU name、buffer group IDs。
6. compare 失败：看 dataset/gold/host output contract。
