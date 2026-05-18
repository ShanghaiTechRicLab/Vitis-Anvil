# HLS 微架构原语

## 这是什么？

`anvil::hls` 是 Vitis-Anvil 项目里一组面向 **Vitis HLS / Xilinx FPGA kernel**
的小型 C++ 头文件库。它**不是算法库**，而是把写 HLS kernel 时反复出现的硬件
结构提炼成可复用、可综合、有静态参数检查的小积木。

每个原语都满足：

- **header-only**，`#include <anvil/hls.hpp>` 即可全部启用。
- **C++14**，可被 Vitis HLS 综合 (`v++`、`vitis_hls`) 和 host 端 g++ 共同编译，
  方便写 SW model 做 co-sim/对拍。
- **零运行时分配**：所有 buffer 都是栈上 `T data[N]`，可以直接被 HLS 综合成
  BRAM/URAM/FIFO/register。
- **HLS pragma 内嵌**：原语在自己内部写好 `#pragma HLS inline`、`pipeline`、
  `unroll`、`array_partition` 等指令，使用者基本不用再补 pragma。
- **静态检查**：所有模板参数（N、Bins、Depth、KeyBits…）通过 `static_assert`
  在编译期校验，写错形状会编译失败而不是综合后发现。

## 功能总览

| 类别          | 原语                                                                                  |
| ------------- | ------------------------------------------------------------------------------------- |
| 定点数        | `fx`、`ufx`、`fixed_traits`、`acc_t`、`saturate_cast`                                 |
| 元操作        | `op::add`、`op::less`、`op::greater`                                                  |
| Tile 与 burst | `mem::tile`、`mem::load_burst`、`mem::store_burst`                                    |
| 多 buffer     | `mem::pingpong`、`mem::multi_buffer`、`mem::triple_buffer`                            |
| 分 bank       | `mem::banked`、`mem::banked_tile`                                                     |
| 局部存储      | `mem::scratchpad`                                                                     |
| 延迟/移位     | `mem::ring`、`mem::shift_register`                                                    |
| 滑窗/行历史   | `mem::line_buffer`、`mem::window_buffer`                                              |
| 归约/前缀     | `compute::reduce`、`tree_reduce`、`sum`、`dot`、`inclusive_scan`、`exclusive_scan`    |
| 直方图/计数排 | `compute::histogram`、`histogram_accumulate`、`count_t`、`counting_sort_reorder`      |
| 排序          | `compute::sort`、`bitonic_sort`、`radix_sort`、`radix_sort_by_key`、`topk`            |
| Dataflow      | `dataflow::dbuf_lcs`                                                                  |

## 怎么用？

```cpp
#include <anvil/hls.hpp>            // 一次引入全部原语
namespace ahls = anvil::hls;        // 推荐 alias，避免长命名空间
```

如果只想引入子集，可以单独包含 `<anvil/hls/mem/tile.hpp>` 之类的细分头文件，
不会引入算法层依赖。

## 适用场景

- 写卷积/池化/累加器：用 `acc_t` 自动算累加位宽，用 `tree_reduce` 做并行求和。
- 写流水线 kernel：用 `tile` + `pingpong` + `dbuf_lcs` 搭 load-compute-store 骨架。
- 写 stencil / 图像滤波：用 `line_buffer` 缓 N 行，配 `window_buffer` 出 NxN 滑窗。
- 写排序 / top-k：N 是 2 的幂用 `bitonic_sort`/`topk`，key 是 `ap_uint` 用
  `radix_sort`，key 域很小用 `counting_sort_reorder`。
- 写直方图、prefix sum、计数等流水化片段：直接套对应原语，不必每次手撸。

---

# 定点数 (`anvil/hls/fixed.hpp`)

## `fx<W, I>` / `ufx<W, I>`

**这是什么**：`ap_fixed` / `ap_ufixed` 的 alias，加默认量化/溢出模式。

**用途**：在 kernel 里替代 `float`/`double`，节省 DSP、降低延迟。

**场景**：DSP、神经网络推理、滤波器系数与样本。

```cpp
using x_t  = ahls::fx<16, 6>;   // 有符号定点：16 位总宽，6 位整数（含符号位）
using ux_t = ahls::ufx<8, 4>;   // 无符号定点：8 位总宽，4 位整数
```

## `fixed_traits<T>`

**这是什么**：定点类型的 trait 萃取，暴露 `width`、`integer`、`fractional`、
`is_signed`、`q_mode`、`o_mode` 等编译期常量。

**用途**：写泛型 HLS 模板时根据输入类型派生中间类型/参数。

```cpp
static_assert(ahls::fixed_traits<x_t>::is_signed, "x_t 应为有符号");
constexpr int frac = ahls::fixed_traits<x_t>::fractional;  // = 10
```

## `acc_t<T, Count, GuardBits = 4>`

**这是什么**：根据「要累加多少个 `T`」自动扩宽出的累加器类型，整数位加上
`ceil_log2(Count) + GuardBits`，保留 T 的符号性。

**用途**：避免累加溢出，又不浪费位宽。

**场景**：MAC、卷积内 reduce、积分图。

```cpp
using x_t   = ahls::fx<16, 6>;
using acc_t = ahls::acc_t<x_t, 256>;   // 累加 256 项，扩 8 位 + 4 guard = +12 位
acc_t acc = 0;
for (int i = 0; i < 256; ++i) {
#pragma HLS pipeline II=1
  acc += static_cast<acc_t>(samples[i]);  // 不会溢出
}
```

## `saturate_cast<To>(value)`

**这是什么**：把 `value` 截到 `To` 的表示范围内，溢出按 `AP_SAT` 饱和而非
回绕。

**用途**：累加器写回窄类型时防止 wrap-around 失真。

```cpp
using out_t = ahls::fx<8, 2>;
out_t y = ahls::saturate_cast<out_t>(acc);  // 超出范围 → 饱和到 out_t 上下界
```

---

# Tile 与 burst (`anvil/hls/mem/tile.hpp`、`burst.hpp`)

## `mem::tile<T, N>`

**这是什么**：一个固定大小的栈上数组的薄封装，附带 `operator[]`。

**用途**：作为 kernel 内的「一小块本地数据」单位，配合 `load_burst`/`store_burst`
做 DDR↔BRAM 搬运。

**场景**：所有 tile-based kernel；按 tile 流水的 GEMM、卷积、向量处理。

```cpp
using word_t = ap_uint<512>;                  // 512-bit AXI burst word
ahls::mem::tile<word_t, 1024> tile;           // 1024 个 word = 64 KB
tile[0] = 1;                                  // 普通下标访问
```

## `mem::load_burst<BurstLen>(in, base, dst)` / `store_burst<BurstLen>`

**这是什么**：从 `T* in` 的 `base` 偏移开始顺序读 N 个元素到 `dst`，每周期
一拍 (`II=1`)。`BurstLen` 是模板参数，对外暴露 burst 长度元信息，便于将来
做综合策略调度。

**用途**：把 DDR 上一段连续数据搬到本地 `tile`，触发 HLS 推断出 AXI burst。

**场景**：所有 `dbuf_lcs` 风格的 load/store 阶段。

**注意**：`base` 单位是 **元素**，不是字节。

```cpp
ahls::mem::tile<word_t, 1024> tile;
ahls::mem::load_burst<16>(in_ptr, tile_id * 1024, tile);   // 读 1024 个 word
// ... compute ...
ahls::mem::store_burst<16>(out_ptr, tile_id * 1024, tile); // 写回
```

---

# 多 buffer (`pingpong.hpp`、`multi_buffer.hpp`、`triple_buffer.hpp`)

## `mem::pingpong<TileT>`

**这是什么**：两个 `TileT` 槽，用 `tile_id & 1` 选择。`read(t)` 和 `write(t)`
返回同一个槽（按 `t` 的奇偶），槽间的交替靠调用者用不同 `tile_id` 实现。

**用途**：在双缓冲 LCS 流水中分离「正在加载」和「正在计算」的 buffer。

**场景**：load-compute、compute-store 之间的解耦。

```cpp
ahls::mem::pingpong<ahls::mem::tile<word_t, 1024>> pp;
auto& load_into  = pp.write(t);       // 当前 tile 写这块
auto& consume_from = pp.read(t - 1);  // 上一个 tile 从另一块读
```

## `mem::multi_buffer<TileT, Slots>`

**这是什么**：`pingpong` 的一般化版本，`Slots` 可以是 2/3/4…。`slot_for(tile_id)`
返回 `tile_id mod Slots` 对应的槽（对负数也正确）。

**用途**：当流水深度需要 ≥3 个并行 buffer 时（例如 prefetch + compute + flush）。

```cpp
ahls::mem::multi_buffer<ahls::mem::tile<int, 64>, 4> mb;
auto& s = mb.slot_for(tile_id);       // 4 槽轮转
auto& s0 = mb.slot<0>();              // 静态索引拿固定槽
```

## `mem::triple_buffer<TileT>`

**这是什么**：`multi_buffer<TileT, 3>` 的封装，提供 `load(t)` / `compute(t)`
/ `store(t)` 三个语义别名（**底层都是同一个 `slot_for`**）。

**用途**：表达三级流水「装载 / 计算 / 回写」的角色，**轮转靠调用者用不同
`tile_id`** —— 典型用法：`load(t)`、`compute(t-1)`、`store(t-2)`。

**坑**：如果三个 stage 都传相同 `tile_id`，会拿到同一个槽（这是 by design，
方便写 stage 等价场景）。

```cpp
ahls::mem::triple_buffer<ahls::mem::tile<int, 64>> tb;
for (int t = 0; t < n_tiles + 2; ++t) {
  if (t < n_tiles)       load_stage   (tb.load(t),       t);     // 槽 t%3
  if (t >= 1 && t <= n_tiles) compute_stage(tb.compute(t-1), t-1); // 槽 (t-1)%3
  if (t >= 2)            store_stage  (tb.store(t-2),    t-2);   // 槽 (t-2)%3
}
```

---

# 分 bank 存储 (`banked.hpp`、`banked_tile.hpp`)

## `mem::banked<T, Depth, Banks>`

**这是什么**：`T data[Banks][Depth]` 的封装，`at(bank, idx)` 双下标访问，
`partition()` 一句把 bank 维度完全 partition。

**用途**：把一块数据按 bank 分开放，让 HLS 综合出多端口 BRAM、做到每周期并
行访问多 bank。

**场景**：systolic array 的权重/激活、向量化 reduce 的 staging buffer。

```cpp
ahls::mem::banked<word_t, 256, 4> banks;   // 4 个 bank，每 bank 深 256
banks.partition();                          // 完全 partition bank 维
banks.at(bank_id, row) = value;             // 写 bank
auto v = banks.at(bank_id, row);            // 读 bank
```

## `mem::banked_tile<T, Depth, Banks>`

**这是什么**：和 `banked` 同样的 `[Banks][Depth]` 布局，但语义偏向「按 tile
切多 bank」，多出 `get<Bank,Idx>()`/`set<Bank,Idx>()`/`fill()`。

**用途**：与 `tile` 同价位、但需要并行访问多个 bank 的本地存储。

**注意**：参数顺序 `<T, Depth, Banks>` 与 `banked` 保持一致；内部仍是
`data[Banks][Depth]`。

```cpp
ahls::mem::banked_tile<int, 256, 4> bt;     // 4 bank × 256 元素
bt.partition_banks();                        // partition bank 维
bt.fill(0);                                  // 全清零
bt.set<0, 0>(42);                            // 编译期下标，零开销
int x = bt.get<0, 0>();
```

---

# 简单本地存储 (`scratchpad.hpp`)

## `mem::scratchpad<T, Depth>`

**这是什么**：单端口 1D 本地数组，提供 `read/write/at/get/set/fill` 等明确
命名的访问 API；`partition_complete()` 把整个数组完全 partition。

**用途**：当不需要分 bank、也不需要双缓冲时的最简单本地存储；让代码意图比
裸 `T arr[Depth]` 更清楚。

```cpp
ahls::mem::scratchpad<word_t, 256> sp;
sp.fill(0);                              // 初始化为 0（II=1 流水循环）
sp.write(i, v);                          // 运行时下标写
v = sp.read(i);                          // 运行时下标读
sp.set<7>(v);                            // 编译期下标写（带越界 static_assert）
auto x = sp.get<7>();                    // 编译期下标读
```

---

# 延迟与移位 (`ring.hpp`、`shift_register.hpp`)

## `mem::ring<T, N>`

**这是什么**：N 槽循环缓冲。`push(v)` 头指针前移、写入新值；`delay<D>()` 读
「D 拍以前的值」（编译期下标，越界编译报错）。

**用途**：表达「保留最近 N 拍历史」的延迟线，无需手算下标。

**场景**：FIR 抽头延迟、IIR 状态、需要回看若干 cycle 的简单时序逻辑。

```cpp
ahls::mem::ring<int, 4> r;
r.push(x);                  // 最新值
int now    = r.delay<0>();  // 当前周期写入的值
int prev   = r.delay<1>();  // 1 拍前
int oldest = r.delay<3>();  // 3 拍前；delay<4> 编译失败
```

## `mem::shift_register<T, Taps...>`

**这是什么**：tap 数固定的移位寄存器。`Shift(v)` 整体左移、新值进入最大 tap；
`Get<Tap>()` 读取指定 tap 位置。

**用途**：FIR/IIR/卷积里位置固定的抽头读取，比 `ring` 更显式表达「我只读这
些固定位置」。

```cpp
ahls::mem::shift_register<int, 0, 1, 2> sr;   // 3 个 tap：位置 0、1、2
sr.Shift(x);                                   // 新值进入 tap 2
int t0 = sr.Get<0>();                          // 最旧
int t2 = sr.Get<2>();                          // 最新（刚 shift 进的 x）
```

---

# 行历史与滑窗 (`line_buffer.hpp`、`window_buffer.hpp`)

## `mem::line_buffer<T, Rows, Cols>`

**这是什么**：`Rows × Cols` 的行历史 buffer。`shift_up(col, v)`：在某一列把
所有行向 row 0 方向上移一格，新值写入最后一行。

**用途**：图像 stencil 处理时缓存「最近 Rows-1 行 + 当前行」，按列推进。

**场景**：3x3/5x5 卷积、Sobel、Gaussian、形态学运算的行 buffer。

```cpp
ahls::mem::line_buffer<sample_t, 3, 1920> lines;  // 3 行 × 1920 列（HD 一行）
lines.partition_rows();                            // 行维 partition，3 行并行
lines.fill(0);

for (int col = 0; col < 1920; ++col) {
#pragma HLS pipeline II=1
  sample_t new_pixel = read_input();
  lines.shift_up(col, new_pixel);                  // 本列：行 0 ← 行 1，行 1 ← 行 2，行 2 ← new
  sample_t r0 = lines.at(0, col);                  // 上面两行历史
  sample_t r1 = lines.at(1, col);
  sample_t r2 = lines.at(2, col);                  // == new_pixel
}
```

## `mem::window_buffer<T, Rows, Cols>`

**这是什么**：完全本地的 `Rows × Cols` 滑窗，所有元素可并行访问。提供
`shift_left(row, v)`（每行整体左移）、`shift_up(col, v)`、和编译期版本。

**用途**：与 `line_buffer` 配合，把当前列从 line_buffer 推进窗口，得到一
个完整的 NxN 邻域用于内核运算。

```cpp
ahls::mem::line_buffer<sample_t, 3, 1920> lines;
ahls::mem::window_buffer<sample_t, 3, 3>   win;
lines.partition_rows();
win.partition_complete();                          // 9 个寄存器全并行

for (int col = 0; col < 1920; ++col) {
#pragma HLS pipeline II=1
  sample_t p = read_input();
  lines.shift_up(col, p);
  // 窗口左移一列，最右列从 line_buffer 取最新 3 行
  for (int r = 0; r < 3; ++r) {
#pragma HLS unroll
    win.shift_left(r, lines.at(r, col));
  }
  sample_t center = win.get<1, 1>();               // 编译期下标读中心
}
```

---

# 归约 (`compute/reduce.hpp`)

## `compute::reduce<Op>(values)`

**这是什么**：用 `Op::apply(acc, v)` 顺序归约 N 个值（线性 reduce，深度 O(N)）。

**用途**：N 较小或不关心延迟时的简单归约；可以传任意符合 `Op` 协议的运算
（`add`、自定义 `max` / `min` 等）。

```cpp
int v[8];
int s = ahls::compute::reduce<ahls::op::add<int>>(v);   // = v[0]+...+v[7]
```

## `compute::tree_reduce<Op>(values)`

**这是什么**：树形归约，O(log N) 深度。对 **任意** N 工作（N 不是 2 的幂时，
落单元素直接进入下一级，不丢数据）。

**用途**：累加器、并行 max/min、并行 popcount —— 时序受限时优先选这个。

```cpp
int v[5];                                                // 注意：5 不是 2 的幂，也能用
int s = ahls::compute::tree_reduce<ahls::op::add<int>>(v);
```

## `compute::sum<N, AccT>(values)` / `dot<N, AccT>(x, w)`

**这是什么**：常用 reduce 的快捷封装。`sum` 累加，`dot` 累加 `x[i]*w[i]`。
`AccT` 由调用方指定，应使用 `acc_t<...>` 选合适位宽。

**用途**：写 MAC / 内积 / 简单累加时少写一个 reduce 模板。

```cpp
using acc_t = ahls::acc_t<ahls::fx<16, 6>, 64>;
acc_t sum_v = ahls::compute::sum<64, acc_t>(values);    // 累加 64 项
acc_t dotxy = ahls::compute::dot<64, acc_t>(x, w);      // ⟨x, w⟩
```

---

# 前缀扫描 (`compute/prefix_sum.hpp`)

## `inclusive_scan<N>(in, out)` / `exclusive_scan<N>(in, out)`

**这是什么**：包含/排除式前缀和。inclusive 的 `out[i] = in[0..i]`；exclusive
的 `out[i] = in[0..i-1]`，`out[0] = 0`。

**用途**：把直方图变成「每个 bin 的起始偏移」、把 flag 变成 compact 后的写
位置（stream compaction）。

**alias-safe**：循环里**先读 `in[i]` 再写 `out[i]`**，因此可以传同一个数组
进去做 in-place 扫描；同名也提供 `prefix_sum_inplace` / `inclusive_scan_inplace`。

```cpp
int in[8]  = {1, 2, 3, 4, 5, 6, 7, 8};
int inc[8], exc[8];
ahls::compute::inclusive_scan<8>(in, inc);  // 1, 3, 6, 10, 15, 21, 28, 36
ahls::compute::exclusive_scan<8>(in, exc);  // 0, 1, 3, 6, 10, 15, 21, 28
ahls::compute::prefix_sum_inplace<8>(in);   // in 变成 exclusive scan 结果
```

## `prefix_sum<N>(in, out)`

**这是什么**：`exclusive_scan` 的别名，命名沿用并行计算社区习惯。

---

# 直方图 (`compute/histogram.hpp`)

## `count_t<N>`

**这是什么**：能容纳 0..N 计数的最窄 `ap_uint`（位宽 = `ceil_log2(N+1)`）。

**用途**：作为直方图 bin 的元素类型，避免用 `int` 浪费位宽。

## `histogram_reset<Bins>(counts)`

**这是什么**：把 `counts[0..Bins)` 全清零（unroll 循环）。

## `histogram_accumulate<KeyBits, Bins>(keys, counts)`

**这是什么**：把 `keys[i]` 当 bin 下标，给对应 `counts[bin]++`；忽略 `bin >= Bins` 的 key。

**性能注意**：`counts[bin]` 有 **read-modify-write 依赖**，HLS 不能稳定达到
II=1；冲突 bin 会让吞吐降低。需要更高吞吐时改成 radix pass 或两阶段：先按
bin 散列再并行归并。

## `histogram<KeyBits, Bins>(keys, counts)`（reset + accumulate 合一）

```cpp
ap_uint<2> keys[8] = {0, 1, 1, 3, 2, 0, 1, 0};
ahls::compute::count_t<8> counts[4];               // 4 bin，最多计 8
ahls::compute::histogram<2, 4>(keys, counts);      // counts = {3, 3, 1, 1}
```

## `histogram<KeyT, N, NumBins, BinPolicy>(keys, bins)`（自定义 binning）

**用途**：key 不是 `ap_uint` 时（例如 `ap_fixed`、自定义类型）通过 policy
计算 bin 索引。

```cpp
using key_t = ahls::ufx<8, 4>;
struct LowBitsBin {
  static int bin(key_t v) { return static_cast<int>(v) & 3; }
};
key_t keys[8];
ahls::compute::count_t<8> bins[4];
ahls::compute::histogram<key_t, 8, 4, LowBitsBin>(keys, bins);
```

**约束**：`Bins ≤ 256`（unroll/partition 的硬限制）；`KeyBits ≤ 30`（避免
`int` bin 下标溢出）。更大 bin 数需要 BRAM-backed histogram，目前未实现。

---

# 计数排序 (`compute/counting_sort.hpp`)

## `counting_sort_reorder<KeyBits, Bins, N, PayloadT>(keys, payloads, out_keys, out_payloads)`

**这是什么**：稳定的全域 counting sort：先 histogram，再 exclusive scan 出
每个 bin 的起始位置，再左→右扫描散列写出 keys+payloads。

**用途**：当 key 域很小（≤256）、N 不大时，O(N) 完成稳定排序，比 bitonic
更省资源。

**约束**：`Bins == 2^KeyBits`（必须全域，保证每条记录都被写出一次）；
输入/输出数组**不能 alias**。

**场景**：低位 radix pass 的核心、桶排序、按类别分桶（同类聚集）。

```cpp
ap_uint<2> keys[8]      = {3, 1, 2, 0, 1, 3, 0, 1};
int        payloads[8]  = {30, 10, 20, 0, 11, 31, 1, 12};
ap_uint<2> ok[8];
int        op[8];
ahls::compute::counting_sort_reorder<2, 4>(keys, payloads, ok, op);
// ok = {0, 0, 1, 1, 1, 2, 3, 3}，相同 key 的 payload 保持原顺序
```

---

# 排序与 top-k (`compute/sort.hpp`、`topk.hpp`、`radix_sort.hpp`)

## `compute::bitonic_sort<N, T, Compare>(values)` / `sort<N>` / `sort<Compare, N>`

**这是什么**：固定大小的 bitonic sort，完全 unroll，O(log²N) 比较深度。

**用途**：小规模（N≤128 量级）并行排序，时序与资源平衡好。

**约束**：N 必须是 **2 的幂**（`static_assert` 卡死）。

```cpp
int v[16];
ahls::compute::sort<16>(v);                                       // 默认升序
ahls::compute::sort<ahls::op::greater<int>, 16>(v);               // 降序
```

## `compute::topk<N, K>(in, out)` / `topk<Compare, N, K>`

**这是什么**：先 bitonic 排序整个 `in[N]`，再取前 K 个。

**用途**：神经网络分类输出取 top-K、检索打分取前 K 等。

**约束**：N 是 2 的幂、`K ≤ N`。默认 `op::less` 取最小 K 个；要取最大 K 个
传 `op::greater<T>`。

```cpp
int scores[16];
int best4[4];
ahls::compute::topk<16, 4>(scores, best4);                        // 最小 4 个
ahls::compute::topk<ahls::op::greater<int>, 16, 4>(scores, best4);// 最大 4 个
```

## `compute::radix_sort<KeyBits, RadixBits, N>(keys)` / `radix_sort_by_key<...>(keys, payloads)`

**这是什么**：LSD radix sort。每个 pass 用 histogram + exclusive scan + 左
→右散列；带 payload 版本是 stable 的。

**用途**：N 不必是 2 的幂、key 是 `ap_uint<KeyBits>`、key 较宽（如 12/16 位）
时的排序首选。

**约束**：`0 < RadixBits ≤ KeyBits`、`RadixBits ≤ 8`（每 pass 直方图最多
256 bin）；通过率受 histogram RAW 依赖限制（非 II=1）。

```cpp
ap_uint<12> keys[64];
int         payloads[64];
ahls::compute::radix_sort_by_key<12, 4>(keys, payloads); // 12 位 key、每 pass 4 位 = 3 pass，stable
ahls::compute::radix_sort<8, 4>(keys8);                  // 仅排 key
```

---

# 元操作 (`anvil/hls/op.hpp`)

| 类型           | 协议方法                              | 用途               |
| -------------- | ------------------------------------- | ------------------ |
| `op::add<T>`   | `identity()`、`apply(a, b) -> a + b`  | reduce、tree_reduce |
| `op::less<T>`  | `before(a, b) -> a < b`               | sort、topk          |
| `op::greater<T>` | `before(a, b) -> a > b`             | sort、topk 降序     |

可以自己定义符合协议的 `struct`（例如 `op::max`、`op::xor_`）传给同类原语。

---

# Double-buffered LCS (`dataflow/dbuf_lcs.hpp`)

## `dataflow::dbuf_lcs<Policy>(in, out, n_tiles)`

**这是什么**：一个 load-compute-store 三阶段流水的骨架函数，通过 `Policy`
struct 定义三个阶段，内部用两个 `pingpong<tile_t>` 解耦输入/输出。

**用途**：写 tile-based kernel 时直接套用，减少 boilerplate；显式表达 LCS
解耦关系。

**当前实现状态**：**correctness skeleton**。它正确地分离 ping-pong bank，
但**没有**在阶段之间加 `pragma HLS dataflow` 或循环 pipeline；这是有意为之
（避免在 ping-pong 还没准备好的情况下误报硬件重叠）。后续会迭代到真正
dataflow 实现。

```cpp
struct MyPipeline {
  using input_t  = const word_t*;
  using output_t = word_t*;
  using tile_t   = ahls::mem::tile<word_t, 1024>;

  static void load   (input_t  in,  int t, tile_t& dst);                 // DDR -> dst
  static void compute(int t, tile_t& src, tile_t& dst);                  // 处理 src -> dst
  static void store  (output_t out, int t, const tile_t& src);           // src -> DDR
};

ahls::dataflow::dbuf_lcs<MyPipeline>(in_ptr, out_ptr, n_tiles);
```

---

# Include 子集

如果只用 mem 层：

```cpp
#include <anvil/hls/mem/tile.hpp>
#include <anvil/hls/mem/burst.hpp>
#include <anvil/hls/mem/pingpong.hpp>
```

如果只用 compute 层：

```cpp
#include <anvil/hls/compute/reduce.hpp>
#include <anvil/hls/compute/sort.hpp>
```

`<anvil/hls.hpp>` 是一站式入口，会把所有子模块都拉进来。

---

# 组件 kernel（综合冒烟测试）

每个原语在 `tests/hls_components/<name>/` 下都有最小 HLS kernel + cosim
testbench，可单独跑 csynth / cosim / analyze 来验证综合质量：

```bash
make csynth-component  TARGET=u250 COMPONENT=tile_burst
make cosim-component   TARGET=u250 COMPONENT=pingpong_dbuf_lcs
make csynth-component  TARGET=u250 COMPONENT=radix_sort
make cosim-component   TARGET=u250 COMPONENT=line_window
make csynth-component  TARGET=u250 COMPONENT=prefix_histogram
make cosim-component   TARGET=u250 COMPONENT=counting_sort_buffer
make analyze-component TARGET=u250 COMPONENT=tile_burst
```

目前可用的 `COMPONENT` 列表：`tile_burst`、`pingpong_dbuf_lcs`、`radix_sort`、
`line_window`、`prefix_histogram`、`counting_sort_buffer`。这些组件**不是
教程示例**，而是 CI 上跑的最小综合 sample，遇到问题时可以参考它们的 kernel
源码作为最小可工作模板。
