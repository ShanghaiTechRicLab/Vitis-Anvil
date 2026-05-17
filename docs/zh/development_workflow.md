# 开发流程

核心规则：先用最便宜的检查抓 bug，再用更慢的 FPGA 步骤。一个五分钟的 CPU 测试胜过一次两小时的 xclbin 重构建。

## 1. 调试阶梯

永远从便宜到昂贵依次推进：

```text
gold/单元测试 → HLS model (make test) → csynth → cosim → xclbin → swemu → hwemu → hw
```

| 阶梯 | 命令 | 能抓住什么 | 抓不住什么 |
|---|---|---|---|
| Gold 和 CPU 单元测试 | `make test` | 普通 C++/Python 错误、数据格式 bug | 任何 FPGA 特定的问题 |
| HLS model | `make test`（已包含） | pack/stream/dataflow/尾部元素 bug | timing、资源、RTL 正确性、XRT |
| csynth | `make csynth` + `make analyze` | HLS 不兼容 C++、II 违例、timing、资源 | RTL vs C++ 差异、host/XRT bug |
| cosim | `make cosim` | RTL 行为和 C++ kernel 不一致、接口协议 bug | Host/XRT bug、platform 特定问题 |
| xclbin | `make xclbin` + `make analyze-link` | 连接错误、内存 bank 不匹配 | 运行时 host bug |
| swemu | `make swemu` | Host app API、BO 设置、参数传递 | RTL 正确性（sw_emu 用的是 C 模型） |
| hwemu | `make hwemu` | RTL 正确性、接口 timing | 物理 timing、卡特定 bug |
| hw | `make hw` | 以上所有 + 真实硬件问题 | — |

不要从改完 kernel 直接跳到上板运行。等几小时只是为了发现一个参数顺序错了，是可以避免的。

---

## 2. 改了普通 C++ 或 Python 代码

```bash
make test
```

这是完整的 CPU-only 测试套件：gold、HLS model、工具和测试。也是第一个 FPGA 形态的检查。应该几分钟内完成。

如果 Python 工具改了：

```bash
make python-env
make test
```

---

## 3. 改了 kernel

跑完整的综合和 cosim 阶梯：

```bash
make test                                       # 先抓 CPU 侧 bug
make csynth TARGET=u250 KERNEL=<kernel>
make analyze TARGET=u250 KERNEL=<kernel>        # 检查 II、timing、资源
make cosim  TARGET=u250 KERNEL=<kernel>
make analyze-cosim TARGET=u250 KERNEL=<kernel>
```

**Kernel 文件职责（以 saxpy 为例）：**

| 文件 | 职责 | 改了会触发什么 |
|---|---|---|
| `src/gold/cpp/saxpy_gold.cpp` | CPU 真值 | csynth/cosim 不应该改变 |
| `src/kernels/include/kernels/saxpy_core.hpp` | 共享核心：Load/Compute/Store + op | HLS model 和 Vitis top 都会变 |
| `src/kernels/saxpy_kernel.cpp` | Vitis top：ABI、pragma、dataflow | csynth + cosim |
| `src/hls_model/saxpy_hls_model.cpp` | 镜像 kernel 结构的 CPU 模型 | 只影响 CPU 测试 |
| `src/host/run_saxpy.cpp` | XRT host | host build + 硬件运行 |

**失败解读：**

| 失败 | 含义 | 解决 |
|---|---|---|
| csynth 编译错 | Kernel 用了 HLS 不支持的 C++ 或 include 缺失 | 修 kernel 代码或 CMake include 路径 |
| II > 1 | 流水线有循环携带依赖或内存端口冲突 | 重构循环、加 `#pragma HLS array_partition`、或降低 II 目标 |
| Timing slack 为负 | 逻辑路径超过了时钟周期 | 降低时钟频率、加 pipeline pragma 重定时、或简化逻辑 |
| cosim 失败 | RTL 行为和 C++ kernel 不一致 | 调试 kernel 逻辑或 testbench；不要继续跑 xclbin |

---

## 4. 改了 host app

```bash
make build TARGET=u250 HOST_APP=<app>
```

这只编译 C++ host，不综合 kernel。

Host app 编译通了但运行失败时，按这个顺序排查：

1. **XRT 没有 source** — 跑 `. /opt/xilinx/xrt/setup.sh` 和 `xbutil examine`
2. **Xclbin 路径错** — 确认 `make xclbin` 成功完成
3. **Compute unit 名** — 必须和 `link.cfg` `nk=` 实例名完全一致
4. **BO group index** — 必须和 kernel C++ 签名里指针参数的顺序对应
5. **数据集路径** — `data/<dataset>/` 是不是 `make gen` 已填充的？

---

## 5. 改了 link.cfg

```bash
make xclbin TARGET=u250
make analyze-link TARGET=u250 HOST_APP=run_saxpy
```

`link.cfg` 改动不会被 C++ 单元测试或 cosim 发现。Vitis linker 会发现 bank 名错（link 报错），host 运行时会发现 compute unit 名错（"kernel not found"）。

改完 `link.cfg` 之后，**在碰 host app 之前**先跑 `analyze-link`。

---

## 6. 添加新的板卡 target

先只做配置和综合，不要直接跑硬件：

```bash
make csynth  TARGET=<target> KERNEL=saxpy
make analyze TARGET=<target> KERNEL=saxpy
```

综合通过了再依次往下：

```bash
make xclbin       TARGET=<target>
make analyze-link  TARGET=<target> HOST_APP=run_saxpy
make build        TARGET=<target> HOST_APP=run_saxpy
make swemu        TARGET=<target> HOST_APP=run_saxpy DATASET=tiny
make hwemu        TARGET=<target> HOST_APP=run_saxpy DATASET=tiny
make hw           TARGET=<target> HOST_APP=run_saxpy DATASET=tiny  # 如果有卡
```

`analyze-link` 是 host 之前的检查点，确认 xclbin 有效且端口绑定符合预期。

嵌入式 target 需要在 host build 之前设置 `PETALINUX_SYSROOT`。

---

## 7. Emulation 工作流细节

Software emulation 前确保 emconfig 文件存在：

```bash
make emconfig TARGET=u250 MODE=sw_emu  # 每个 target 生成一次
make swemu    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

Hardware emulation：

```bash
make emconfig TARGET=u250 MODE=hw_emu  # 和 sw_emu 同一文件，已有就跳过
make hwemu    TARGET=u250 HOST_APP=run_saxpy DATASET=tiny
```

hw_emu 用非常小的数据集。大数据的 RTL 仿真非常慢。

Emulation 失败时的诊断：
- `swemu` 通过但 `hwemu` 失败：RTL 有 bug，C 模型没发现。重跑 cosim。
- `swemu` 和 `hwemu` 都失败：host app 有 bug，和 RTL 无关。调试 BO 设置。
- `hwemu` 通过但 `hw` 失败：可能是物理 timing 或板卡特定问题。

---

## 8. 提交策略

好的提交小而分层：

1. Gold/reference 改动 + 测试
2. Kernel ABI/头文件改动
3. Kernel 实现 + cosim testbench
4. CMake/Make 注册
5. Host app 改动
6. 板卡配置 / link.cfg 改动
7. 文档更新

除非无法分开，不要把"新 kernel 实现"和"新板卡配置"放在同一个提交里。

---

## 9. 重大改动后要记录什么

任何 kernel 或板卡改动后，记录：

- 跑了哪些命令、测试了哪些 target
- csynth II 和 timing 是否达标
- Cosim 是否通过
- 是否跑了硬件 link
- 是否跑了真实硬件，或者用的哪种 emulation mode
- 任何已知未测试的路径（旧 XRT fallback、嵌入式 sysroot、因为太慢跳过的 hw_emu 等）

这份记录在别人（或将来的自己）接手这个工作时很有用。
