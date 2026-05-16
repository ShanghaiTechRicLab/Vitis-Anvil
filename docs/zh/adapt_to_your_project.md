# 适配到你自己的项目

Vitis-Anvil 常见有两种用法。

## 方案 A：直接作为 project template

1. 复制本 repo。
2. 保留 `cmake/`、`CMakePresets.json`、`Makefile`、`config/`、`tools/hlsflow/`、`python/anvil/`。
3. 替换 demo kernels 和 host apps。
4. 在你的替换测试通过前，保留现有测试作为护栏。
5. 流程稳定后，再重命名产品侧 binary 和文档。

建议替换顺序：

```text
kernel C++ → cosim testbench → CPU model/gold → host app → dataset → compare → xclbin connectivity → board run
```

## 方案 B：把 build modules vendor 到已有 repo

复制这些部分到你的已有项目：

```text
cmake/AnvilKernel.cmake
cmake/AnvilHost.cmake
cmake/FindVitis.cmake
cmake/FindXRT.cmake
cmake/Toolchain-*.cmake
config/<target>/
tools/hlsflow/
```

然后在你的 top-level `CMakeLists.txt` include 这些模块，用 `add_anvil_kernel()` 和 `add_anvil_xclbin()` 注册自己的 kernels。

## 最小 CMake 形状

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_accel LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(AnvilKernel)
include(AnvilHost)

add_subdirectory(src/kernels)
add_subdirectory(src/host)
```

## 最小 Make 用户界面

保持一个小而稳定的用户接口：

```bash
make test
make build TARGET=<board> HOST_APP=<app>
make csynth TARGET=<board> KERNEL=<kernel>
make cosim TARGET=<board> KERNEL=<kernel>
make xclbin TARGET=<board>
make run-host TARGET=<board> HOST_APP=<app>
```

这个接口很容易以后封装成：

```bash
anvil init
anvil build --target u250 --host-app run_saxpy
anvil csynth --target u250 --kernel saxpy
```

## 命名和 packaging

内部模板/工具层使用 `anvil`。最终 accelerator、bitstream package、用户应用使用你的产品名。这样 reusable build flow 和 product identity 不会混在一起。

## 建议保留

保留：

- `TARGET`、`KERNEL`、`HOST_APP`、`DATASET` 的职责拆分
- 显式 Python environment target
- 显式长耗时 HLS/xclbin targets
- HLS report database
- `ANVIL_PLATFORM=` 覆盖 platform path
- `PETALINUX_SYSROOT=` 覆盖 embedded sysroot

替换：

- demo kernels
- demo host apps
- dataset format
- gold 和 compare 逻辑
- connectivity files
- 产品文档
