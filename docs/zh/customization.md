# 自定义指南：把 demo 换成你的加速器

这篇假设你只大概知道 FPGA 可以用 HLS 跑 C/C++，但还不知道一个 Vitis/XRT 工程要分哪些步骤。目标是从"我有一个算法"讲到"我能在板卡上跑并和 CPU 结果比较"。

最重要的规则：

> `include/anvil/**` 和 `src/anvil/**` 是框架代码，默认不要改。你的项目代码放在 `src/kernels/**`、`src/hls_model/**`、`src/gold/**`、`src/host/**`、`src/apps/**`、`config/**`、`tests/**`。

## 0. 自定义到底是在改什么？

一个 FPGA 加速项目不只是一个 kernel 函数。它至少包含这些部分：

| 部分 | 做什么 | 放在哪里 | 可编辑？ |
|---|---|---|---|
| 框架公开 API | 通用 helper：log、JSON、compare、runtime wrapper、hlslib wrapper（`pack.hpp`、`stream.hpp` 等） | `include/anvil/**` | 否 |
| 框架实现 | 框架库的 C++ 实现 | `src/anvil/**` | 否 |
| Kernel ABI 常量 | pack 宽度常量，不含 Vitis/hlslib 头文件，host 编译可用 | `src/kernels/include/kernels/abi.hpp` | 是 |
| Kernel pack 类型 | 使用 `anvil::hls::Pack` 的 Pack typedef，含 HLS 头文件 | `src/kernels/include/kernels/kernel_types.hpp` | 是 |
| Kernel ABI 头文件 | `extern "C"` 声明、共享 kernel-core helper、操作函子 | `src/kernels/include/kernels/*.hpp` | 是 |
| Kernel 实现 | Vitis HLS C++ kernel（Vitis top：pragma + dataflow） | `src/kernels/*.cpp` | 是 |
| HLS/CPU 模型 | CPU 编译的模型，结构镜像 kernel | `src/hls_model/**` | 是 |
| Golden reference | CPU 正确性实现和指标 | `src/gold/**` | 是 |
| Host app | 加载 xclbin、运行 kernel 的 XRT 程序 | `src/host/**` | 是 |
| 工具 | 数据集生成、比较、部署、分析 helper | `src/apps/**`、`scripts/**` | 是 |
| 板卡/platform 配置 | xpfm 路径、part、sysroot、link.cfg、xrt.ini、元数据 | `config/<target>/**`、`platforms/**` | 是 |
| 测试 | C++ 测试、Python 测试、cosim testbench、install smoke | `tests/**` | 是 |

### 两文件 kernel 类型模式

Demo kernel 刻意把类型定义拆成两个文件：

- **`abi.hpp`** — 只有宽度常量（如 `kSaxpyPackWidth = 16`），不含 Vitis 或 hlslib 头文件。host 交叉编译安全。
- **`kernel_types.hpp`** — Pack typedef（如 `SaxpyPack = anvil::hls::Pack<float, 16>`），include `anvil/hls/pack.hpp` 和 `abi.hpp`。仅供 kernel 和模型代码使用。

对你自己的 kernel 也推荐这样做：创建 `abi.hpp` 放宽度常量，再定义 pack 类型。小项目可以合并成一个文件 — 但要知道主头文件 include HLS 头文件后，交叉编译 host 会失败。

### 框架 HLS helper 头文件

在你的 kernel 中可以 include 这些框架头文件：

```cpp
#include "anvil/hls/pack.hpp"       // Pack<T,N>、PackTraits、GetLane、SetLane
#include "anvil/hls/stream.hpp"     // Stream<T,Depth>、kDefaultDataflowStreamDepth
#include "anvil/hls/dataflow.hpp"   // ANVIL_DATAFLOW_* 宏
#include "anvil/hls/packed_ops.hpp" // LoadPacks、StorePacks、MapPacksWithScalar、MapMem2Packs
#include "anvil/hls/axis.hpp"       // WriteAxis、ReadAxis（k2k stream pipeline 用）
```

不要把自己的 kernel 类型加到 `include/anvil/`。放在 `src/kernels/include/kernels/`。

## 1. 四大变量：TARGET、KERNEL、HOST_APP、DATASET

日常命令大多由这四个变量控制：

| 变量 | 选什么 | 例子 |
|---|---|---|
| `TARGET` | 板卡/platform 配置（`config/<target>/anvil.mk`） | `u250`, `u50`, `zcu102`, `kv260` |
| `KERNEL` | HLS kernel 目标名 | `saxpy`, `vadd`, `pipeline_demo`, `all` |
| `HOST_APP` | host 可执行文件名（`src/host/CMakeLists.txt`） | `run_saxpy`, `run_vadd` |
| `DATASET` | 数据目录（`data/<dataset>/`） | `tiny`, `my_case_001` |

不要混用。`HOST_APP` 不选择 cosim；`KERNEL` 才选择 cosim。

## 2. 推荐顺序

按这个顺序做可以少等很多 Vitis 慢步骤：

1. 写清楚算法公式和输入输出。
2. 定义 kernel ABI — pack 宽度、pack 类型、`extern "C"` 签名 — 放在 `src/kernels/include/kernels/`。
3. 写共享 kernel core — Load/Compute/Store wrapper 和操作函子 — 放在同一或单独的 `*_core.hpp`。
4. 写 CPU gold reference。
5. 给 gold 写 CPU 测试。
6. 写 HLS kernel，从 `anvil/hls/` include 框架 helper。
7. 写 cosim testbench。
8. 在 `src/kernels/CMakeLists.txt` 注册 kernel。
9. 跑 `make test`，先保证 CPU 侧没坏。
10. 跑 `make csynth TARGET=<target> KERNEL=<kernel>`。
11. 跑 `make cosim TARGET=<target> KERNEL=<kernel>`。
12. 写/改 `link.cfg`。
13. 跑 `make xclbin TARGET=<target>`。
14. 写/改 host app。
15. 生成 dataset、跑 gold、跑 host、compare。
16. 最后再调性能：clock、pack 宽度、memory bank、dataflow 深度。

## 3. 完整例子：添加 `scaleadd`

我们添加一个新 kernel：

```text
out[i] = alpha * a[i] + beta * b[i]
```

它有两个输入数组 `a`、`b`，一个输出数组 `out`，两个标量参数 `alpha`、`beta`。

### 3.1 先定义类型

创建 `src/kernels/include/kernels/scaleadd_types.hpp`：

```cpp
#pragma once
// ScaleAdd kernel 类型定义。遵循两文件模式：
//   abi.hpp      — 仅宽度常量，host 编译安全
//   types        — Pack typedef，使用 anvil::hls::Pack（含 HLS include）

#include "anvil/hls/pack.hpp"

namespace kernels {

static const int kScaleAddPackWidth = 16;
typedef anvil::hls::Pack<float, kScaleAddPackWidth> ScaleAddPack;

}  // namespace kernels
```

小 kernel 可以把宽度常量和 pack typedef 合在一个文件。大项目建议拆开，让 host 交叉编译不拉 HLS 头文件。

### 3.2 定义 ABI 头文件和操作函子

创建 `src/kernels/include/kernels/scaleadd.hpp`：

```cpp
#pragma once

#include "kernels/scaleadd_types.hpp"

extern "C" void scaleadd(const kernels::ScaleAddPack* a,
                         const kernels::ScaleAddPack* b,
                         kernels::ScaleAddPack* out,
                         float alpha,
                         float beta,
                         int n_packs);
```

创建 `src/kernels/include/kernels/scaleadd_op.hpp`：

```cpp
#pragma once

namespace kernels {

struct ScaleAddOp {
  float alpha = 1.0f;
  float beta = 1.0f;
  float operator()(float a, float b) const { return alpha * a + beta * b; }
};

}  // namespace kernels
```

这一步做什么：

- `scaleadd.hpp` 定义 `extern "C"` 签名，是 kernel、cosim testbench、host app 之间的 ABI 契约。
- `scaleadd_types.hpp` 定义 pack 类型，kernel 和模型共享。
- `scaleadd_op.hpp` 定义计算函子，kernel 和模型共享。
- 这些文件放在 `src/kernels/include/kernels/`，因为它们是项目代码，不是框架 API。

### 3.3 写 HLS kernel

创建 `src/kernels/scaleadd_kernel.cpp`：

```cpp
#include "kernels/scaleadd.hpp"

#include "anvil/hls/pack.hpp"
#include "anvil/hls/packed_ops.hpp"
#include "kernels/scaleadd_op.hpp"
#include "kernels/scaleadd_types.hpp"

extern "C" void scaleadd(const kernels::ScaleAddPack* a,
                         const kernels::ScaleAddPack* b,
                         kernels::ScaleAddPack* out,
                         float alpha,
                         float beta,
                         int n_packs) {
#pragma HLS INTERFACE m_axi port=a   bundle=gmem0 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=b   bundle=gmem1 offset=slave depth=1024
#pragma HLS INTERFACE m_axi port=out bundle=gmem2 offset=slave depth=1024
#pragma HLS INTERFACE s_axilite port=a       bundle=control
#pragma HLS INTERFACE s_axilite port=b       bundle=control
#pragma HLS INTERFACE s_axilite port=out     bundle=control
#pragma HLS INTERFACE s_axilite port=alpha   bundle=control
#pragma HLS INTERFACE s_axilite port=beta    bundle=control
#pragma HLS INTERFACE s_axilite port=n_packs bundle=control
#pragma HLS INTERFACE s_axilite port=return  bundle=control

  kernels::ScaleAddOp op;
  op.alpha = alpha;
  op.beta = beta;
  anvil::hls::MapMem2Packs(a, b, out, n_packs, op);
}
```

每个 pragma 的含义：

- `m_axi`：大块内存接口，给数组指针用。
- `s_axilite`：控制寄存器接口，给标量和指针地址用。
- `MapMem2Packs` 对每个 lane 应用函子，自动带 `#pragma HLS pipeline II=1` 和 `#pragma HLS unroll`。

`n_packs` 是 pack 数，不是 float 元素数。

如果需要内部 dataflow（Load → Compute → Store），参考 saxpy 模式：在 `*_core.hpp` 中创建非模板 Load/Compute/Store wrapper，通过 `ANVIL_DATAFLOW_*` 宏调用。详见 [hlslib 适配](hlslib_adaptation.md)。

### 3.4 写 cosim testbench

创建 `tests/kernels/scaleadd_cosim_tb.cpp`：

```cpp
#include "kernels/scaleadd.hpp"
#include "kernels/scaleadd_types.hpp"

#include <cstdio>
#include <vector>

namespace {

constexpr int kPacks = 4;
constexpr int kInterfaceDepthPacks = 1024;

int RunOne() {
  std::vector<kernels::ScaleAddPack> a(kInterfaceDepthPacks);
  std::vector<kernels::ScaleAddPack> b(kInterfaceDepthPacks);
  std::vector<kernels::ScaleAddPack> out(kInterfaceDepthPacks);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
      a[p][lane] = static_cast<float>(p * kernels::kScaleAddPackWidth + lane);
      b[p][lane] = 10.0f;
      out[p][lane] = 0.0f;
    }
  }

  const float alpha = 2.0f;
  const float beta = 3.0f;
  scaleadd(a.data(), b.data(), out.data(), alpha, beta, kPacks);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
      const float expected = alpha * a[p][lane] + beta * b[p][lane];
      if (out[p][lane] != expected) {
        std::printf("FAIL p=%d lane=%d got=%g expected=%g\n",
                    p, lane, out[p][lane], expected);
        return 1;
      }
    }
  }
  return 0;
}

}  // namespace

int main() {
  const int rc = RunOne();
  std::printf("scaleadd cosim: %s\n", rc == 0 ? "PASS" : "FAIL");
  return rc;
}
```

这一步做什么：

- 不用 host app，不用 xclbin。
- 直接调用 kernel top function。
- Vitis cosim 会用它验证生成的 RTL 是否正确。

### 3.5 注册 kernel

编辑 `src/kernels/CMakeLists.txt`，添加：

```cmake
set(_scaleadd_kernel_args
  NAME          scaleadd
  TOP           scaleadd
  SOURCES       scaleadd_kernel.cpp
  PLATFORM_KIND ${ANVIL_PLATFORM_KIND})

if(ANVIL_BUILD_TESTS)
  list(APPEND _scaleadd_kernel_args
    TESTBENCH ${CMAKE_SOURCE_DIR}/tests/kernels/scaleadd_cosim_tb.cpp)
endif()

add_anvil_kernel(${_scaleadd_kernel_args})
```

要不要放进默认 xclbin：

```cmake
if(ANVIL_PLATFORM_KIND MATCHES "^(zcu104|zcu102|zcu106|kv260)$")
  set(_saxpy_xclbin_kernels saxpy_xo)
else()
  set(_saxpy_xclbin_kernels saxpy_xo vadd_xo scaleadd_xo)
endif()
```

### 3.6 添加 link.cfg

编辑 `config/u250/link.cfg`：

```ini
[connectivity]
nk=scaleadd:1:scaleadd_1
sp=scaleadd_1.a:DDR[0]
sp=scaleadd_1.b:DDR[1]
sp=scaleadd_1.out:DDR[2]

[clock]
freqHz=300000000:scaleadd_1
```

含义：

- `nk` 创建 compute unit，名字是 `scaleadd_1`。
- `sp` 把指针参数绑定到 DDR bank。
- `alpha`、`beta`、`n_packs` 是 scalar，不需要 `sp=`。

host app 之后要用同一个名字：

```cpp
ctx.GetKernel("scaleadd:{scaleadd_1}");
```

### 3.7 运行 synthesis 和 cosim

```bash
make csynth TARGET=u250 KERNEL=scaleadd
make analyze TARGET=u250 KERNEL=scaleadd
make cosim TARGET=u250 KERNEL=scaleadd
make analyze-cosim TARGET=u250 KERNEL=scaleadd
```

如果 `csynth` 失败，问题通常在 HLS 代码、include path、platform。
如果 `cosim` 失败，问题通常在 kernel 行为或 testbench。

### 3.8 写 host app

创建 `src/host/run_scaleadd.cpp`，它要做这些事：

1. 解析 `--xclbin`、`--n`、`--alpha`、`--beta`。
2. 准备输入数组。
3. 打开 `XrtContext`。
4. 用 `scaleadd:{scaleadd_1}` 找 kernel。
5. 分配三个 buffer：`a`、`b`、`out`。
6. 把输入拷到 device。
7. 调用 kernel。
8. 把输出拷回。
9. 写输出文件或直接检查。

注册到 `src/host/CMakeLists.txt`：

```cmake
add_anvil_host(
    NAME run_scaleadd
    SOURCES run_scaleadd.cpp
    LINK_LIBRARIES
        anvil_runtime
        anvil_log
        anvil_cli
        anvil_json
        anvil_compare)

target_include_directories(run_scaleadd PRIVATE ${PROJECT_SOURCE_DIR}/src/kernels/include)
```

构建：

```bash
make build TARGET=u250 HOST_APP=run_scaleadd
```

注意：BO group index 必须和 kernel 指针参数顺序一致。`scaleadd(a, b, out, alpha, beta, n_packs)` 中 host 可见 memory 参数是：

| 参数 | BO group |
|---|---:|
| `a` | 0 |
| `b` | 1 |
| `out` | 2 |

## 4. 自定义数据集和 gold

如果你要完整比较 run 输出，需要数据格式。先定义 `meta.json`，例如：

```json
{
  "case_id": "scaleadd_tiny_001",
  "n": 1024,
  "alpha": 2.0,
  "beta": 3.0,
  "a": "a.bin",
  "b": "b.bin",
  "gold": "gold_out.bin"
}
```

然后让这些组件都使用同一套约定：

| 组件 | 要做什么 |
|---|---|
| dataset generator | 写 `a.bin`、`b.bin`、`meta.json` |
| gold reference | 读 `meta.json` 和输入，写 `gold_out.bin` |
| host app | 读输入，运行硬件，写 Make target 传入的 `--output` 路径 |
| compare | 比较 `gold_out.bin` 和 run 输出 |

不要让每个程序各自发明文件名。host 忽略 `--output` 会让你误以为 kernel 错了。

## 5. 添加板卡

添加板卡不是只改一个路径。你需要：

1. 新建 `config/<target>/anvil.mk`。
2. 写正确的 `ANVIL_PLATFORM` `.xpfm` 路径。
3. 写 `link.cfg`，确认 memory bank 名和 platform 匹配。
4. 写 `xrt.ini`。
5. 在 `CMakePresets.json` 添加 configure/build preset。
6. 如果要分析报告显示资源百分比，更新 `tools/hlsflow/platform_info.py`。
7. 写 `config/<target>/README.md` 记录 Vitis 版本、platform 来源、XRT 设置。

加速卡 `anvil.mk` 例子：

```make
ANVIL_DEVICE_KIND    := accelerator
ANVIL_PLATFORM       ?= /path/to/platform.xpfm
ANVIL_PRESET         := my-card-host
ANVIL_HWEMU_PRESET   := my-card-host-hwemu
ANVIL_NEEDS_CROSS    := no
ANVIL_XCLBIN_MODE    := hw
ANVIL_KERNEL_TARGETS := saxpy_xo vadd_xo scaleadd_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim vadd_cosim scaleadd_cosim
```

Embedded `anvil.mk` 例子：

```make
ANVIL_DEVICE_KIND    := embedded
ANVIL_NEEDS_CROSS    := yes
ANVIL_SYSROOT        ?= $(PETALINUX_SYSROOT)
ANVIL_PRESET         := my-board-kernel
ANVIL_HOST_PRESET    := my-board-host
ANVIL_PLATFORM       ?= /path/to/embedded/platform.xpfm
ANVIL_KERNEL_TARGETS := saxpy_xo
ANVIL_COSIM_TARGETS  := saxpy_cosim
```

## 6. 提交前检查清单

- [ ] 项目专用头文件没有放到 `include/anvil/**`。
- [ ] kernel 参数顺序、host BO group、`link.cfg sp=` 三者一致。
- [ ] `make test` 通过。
- [ ] `make csynth TARGET=<target> KERNEL=<kernel>` 通过。
- [ ] 有 testbench 的话，`make cosim TARGET=<target> KERNEL=<kernel>` 通过。
- [ ] host app 能 `make build TARGET=<target> HOST_APP=<app>`，并能通过 `make swemu`/`make hwemu`/`make qemu`/`make hw` 写出 run 输出。
- [ ] dataset 文件名在 generator/gold/host/compare 中一致。
- [ ] 文档写清楚板卡环境和运行命令。

## 7. 常见错误

### 找不到 `kernels/my_kernel.hpp`

编译目标缺少 `src/kernels/include` include path。host app 要在 `src/host/CMakeLists.txt` 加 `target_include_directories`。

### cosim 没有配置

kernel 注册时没有 `TESTBENCH`，或者 `config/<target>/anvil.mk` 的 `ANVIL_COSIM_TARGETS` 没有包含 `<kernel>_cosim`。

### xclbin 里找不到 kernel

检查 `link.cfg` 的 `nk=` 名字和 host app 里的 `ctx.GetKernel("name:{cu}")` 是否一致。

### 输出不对

按顺序查：

1. host BO group index
2. kernel 参数顺序
3. element count vs pack count
4. tail lane padding
5. `link.cfg sp=` memory bank
6. dataset 文件是否读错
