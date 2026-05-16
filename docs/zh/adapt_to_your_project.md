# 适配到你自己的项目

当你不再满足于 demo 之后，Vitis-Anvil 通常有两种用法。

## 方案 A：直接作为项目模板

1. 复制本仓库。
2. 保留 `cmake/`、`CMakePresets.json`、`Makefile`、`config/`、`tools/hlsflow/`、`python/anvil/`。
3. 替换 demo kernel 和 host app。
4. 在你自己的测试就绪之前，让现有测试保持通过，作为安全网。
5. 流程稳定后再重命名产品的二进制文件和文档。

建议的替换顺序：

```text
kernel C++ → cosim testbench → CPU 模型/gold → host app → dataset → compare → xclbin connectivity → board run
```

## 方案 B：把构建模块 vendor 到已有项目

把以下文件复制到你已有的项目：

```text
cmake/AnvilKernel.cmake
cmake/AnvilHost.cmake
cmake/FindVitis.cmake
cmake/FindXRT.cmake
cmake/Toolchain-*.cmake
config/<target>/
tools/hlsflow/
```

然后在你的顶层 `CMakeLists.txt` 中 include 这些模块，用 `add_anvil_kernel()` 和 `add_anvil_xclbin()` 注册你自己的 kernel。

## 最小 CMake 结构

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

## 最小的 Make 用户界面

保持一组小而稳定的面向用户的命令：

```bash
make test
make build TARGET=<board> HOST_APP=<app>
make csynth TARGET=<board> KERNEL=<kernel>
make cosim TARGET=<board> KERNEL=<kernel>
make xclbin TARGET=<board>
make run-host TARGET=<board> HOST_APP=<app>
```

这组接口设计得足够小，以后很容易封装成：

```bash
anvil init
anvil build --target u250 --host-app run_saxpy
anvil csynth --target u250 --kernel saxpy
```

## 命名和打包

内部模板和工具层用 `anvil`。最终的加速器、比特流包和面向用户的应用程序用你自己的产品名。这样可复用的构建流程和产品身份就不会混在一起。

## 建议保留和替换

保留：

- `TARGET`、`KERNEL`、`HOST_APP`、`DATASET` 的职责分离
- 显式的 Python 环境目标
- 显式的长耗时 HLS 和 xclbin 目标
- HLS 报告数据库
- 通过 `ANVIL_PLATFORM=` 覆盖 platform 路径
- 通过 `PETALINUX_SYSROOT=` 覆盖嵌入式 sysroot

替换：

- Demo kernel
- Demo host app
- 数据集格式
- Golden reference 和对比逻辑
- Connectivity 文件
- 产品文档
