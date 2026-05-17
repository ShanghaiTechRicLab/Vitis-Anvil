# 自定义指南：把 demo 换成你的加速器

这篇假设你只大概知道 FPGA 可以用 HLS 跑 C/C++，但还不知道一个 Vitis/XRT 工程要分哪些步骤。目标是从“我有一个算法”讲到“我能在板卡上跑并和 CPU 结果比较”。

最重要的规则：

> `include/anvil/**` 和 `src/anvil/**` 是框架代码，默认不要改。你的项目代码放在 `src/kernels/**`、`src/hls_model/**`、`src/gold/**`、`src/host/**`、`src/apps/**`、`config/**`、`tests/**`。

## 1. 自定义到底是在改什么？

一个 FPGA 加速项目不只是一个 kernel 函数。它至少包含这些部分：

| 部分 | 做什么 | 放在哪里 |
|---|---|---|
| Gold reference | CPU 上的正确实现，用来当真值 | `src/gold/**` |
| HLS kernel | 会被综合成 FPGA 硬件的 C++ 函数 | `src/kernels/*.cpp` |
| Kernel ABI header | kernel 函数签名、pack 类型、宽度常量 | `src/kernels/include/kernels/*.hpp` |
| Cosim testbench | 不上板，先验证 RTL 行为 | `tests/kernels/*_cosim_tb.cpp` |
| xclbin connectivity | 告诉 Vitis kernel 实例和内存 bank 怎么连 | `config/<target>/link.cfg` |
| Host app | CPU 程序，加载 xclbin、分配 buffer、启动 kernel | `src/host/*.cpp` |
| Dataset/compare | 输入、期望输出、`runs/` 下的运行输出、比较逻辑 | `data/`, `runs/`, `src/apps/`, `scripts/` |

所以“加一个新算法”不是只写一个 `.cpp`。你要让每一层都知道同一件事：输入是什么、输出是什么、参数顺序是什么、文件名是什么、怎么判断正确。

## 2. 推荐顺序

按这个顺序做可以少等很多 Vitis 慢步骤：

1. 写清楚算法公式和输入输出。
2. 写 CPU gold reference。
3. 给 gold 写 CPU 测试。
4. 写 kernel ABI header。
5. 写 HLS kernel。
6. 写 cosim testbench。
7. 在 `src/kernels/CMakeLists.txt` 注册 kernel。
8. 跑 `make test`，先保证 CPU 侧没坏。
9. 跑 `make csynth TARGET=<target> KERNEL=<kernel>`。
10. 跑 `make cosim TARGET=<target> KERNEL=<kernel>`。
11. 写/改 `link.cfg`。
12. 跑 `make xclbin TARGET=<target>`。
13. 写/改 host app。
14. 生成 dataset、跑 gold、跑 host、compare。
15. 最后再调性能：clock、pack 宽度、memory bank、dataflow 深度。

## 3. 完整例子：添加 `scaleadd`

我们添加一个新 kernel：

```text
out[i] = alpha * a[i] + beta * b[i]
```

它有两个输入数组 `a`、`b`，一个输出数组 `out`，两个标量参数 `alpha`、`beta`。

### 3.1 先定义 ABI 头文件

创建 `src/kernels/include/kernels/scaleadd.hpp`：

```cpp
#pragma once

#include "anvil/hls/pack.hpp"

namespace kernels {

static const int kScaleAddPackWidth = 16;
typedef anvil::hls::Pack<float, kScaleAddPackWidth> ScaleAddPack;

}  // namespace kernels

extern "C" void scaleadd(const kernels::ScaleAddPack* a,
                         const kernels::ScaleAddPack* b,
                         kernels::ScaleAddPack* out,
                         float alpha,
                         float beta,
                         int n_packs);
```

这一步做什么：

- 定义硬件接口中的 packed 数据类型。
- 定义 kernel top function 的参数顺序。
- 给 kernel、cosim testbench、host app 共享同一份声明。

为什么不用 `include/anvil/`：这是你的项目 ABI，不是 Anvil 框架 API。

### 3.2 写 HLS kernel

创建 `src/kernels/scaleadd_kernel.cpp`：

```cpp
#include "kernels/scaleadd.hpp"
#include "anvil/hls/pack.hpp"

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

  for (int i = 0; i < n_packs; ++i) {
#pragma HLS PIPELINE II=1
    kernels::ScaleAddPack pa = a[i];
    kernels::ScaleAddPack pb = b[i];
    kernels::ScaleAddPack po;
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
#pragma HLS UNROLL
      const float av = anvil::hls::GetLane(pa, lane);
      const float bv = anvil::hls::GetLane(pb, lane);
      anvil::hls::SetLane(po, lane, alpha * av + beta * bv);
    }
    out[i] = po;
  }
}
```

这里每个 pragma 的含义：

- `m_axi`：这是大块内存接口，给数组指针用。
- `s_axilite`：这是控制寄存器接口，给标量和指针地址用。
- `PIPELINE II=1`：要求循环尽量每周期处理一个 pack。
- `UNROLL`：把 pack 内 lane 并行展开。

`n_packs` 是 pack 数，不是 float 元素数。如果有 1024 个 float，pack width 是 16，那么 `n_packs = 64`。

### 3.3 写 cosim testbench

创建 `tests/kernels/scaleadd_cosim_tb.cpp`：

```cpp
#include "kernels/scaleadd.hpp"

#include <cstdio>
#include <vector>

int main() {
  const int kPacks = 4;
  const int kDepth = 1024;
  std::vector<kernels::ScaleAddPack> a(kDepth), b(kDepth), out(kDepth);

  for (int p = 0; p < kPacks; ++p) {
    for (int lane = 0; lane < kernels::kScaleAddPackWidth; ++lane) {
      a[p].Set(lane, static_cast<float>(p * kernels::kScaleAddPackWidth + lane));
      b[p].Set(lane, 10.0f);
      out[p].Set(lane, 0.0f);
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

  std::printf("scaleadd cosim: PASS\n");
  return 0;
}
```

这一步做什么：

- 不用 host app，不用 xclbin。
- 直接调用 kernel top function。
- Vitis cosim 会用它验证生成的 RTL 是否正确。

### 3.4 注册 kernel

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

这一步做什么：

- 创建 `scaleadd_xo` CMake target。
- 如果有 `TESTBENCH`，创建 `scaleadd_cosim` target。
- 让 `make csynth TARGET=u250 KERNEL=scaleadd` 知道该构建什么。

### 3.5 运行 synthesis 和 cosim

```bash
make csynth TARGET=u250 KERNEL=scaleadd
make analyze TARGET=u250 KERNEL=scaleadd
make cosim TARGET=u250 KERNEL=scaleadd
make analyze-cosim TARGET=u250 KERNEL=scaleadd
```

如果 `csynth` 失败，问题通常在 HLS 代码、include path、platform。 如果 `cosim` 失败，问题通常在 kernel 行为或 testbench。

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

### 3.7 写 host app

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
