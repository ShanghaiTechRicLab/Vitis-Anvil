# HLS 微架构原语

Vitis-Anvil 在 `anvil::hls` 下提供小型 HLS 微架构原语。这不是算法库，而是 kernel
内部常用硬件结构：定点数、tile buffer、banked buffer、ring buffer、shift register、
line/window buffer、burst load/store、ping-pong buffer、scratch/banked buffer、归约、
prefix scan、histogram、counting-sort reorder、小规模固定排序、radix sort、top-k
选择，以及 double-buffered load-compute-store 骨架。

## Include 风格

```cpp
#include <anvil/hls.hpp>
namespace ahls = anvil::hls;
```

## 定点数

```cpp
using x_t = ahls::fx<16, 6>;
using acc_t = ahls::acc_t<x_t, 256>;
```

`acc_t<T, N>` 会按 `ceil_log2(N)` 加 guard bits 扩宽累加器。

## Tile 和 burst

```cpp
using word_t = ap_uint<512>;
ahls::mem::tile<word_t, 1024> tile;

ahls::mem::load_burst<16>(in, base, tile);
ahls::mem::store_burst<16>(out, base, tile);
```

`base` 的单位是 `word_t` 元素，不是字节。burst 长度放在模板参数里，方便进入综合
action 元数据，也方便后续扩展 loop-shaping policy。

## Ping-pong

```cpp
ahls::mem::pingpong<ahls::mem::tile<word_t, 1024>> pp;
auto& load_tile = pp.write(tile_id);
auto& compute_tile = pp.read(tile_id - 1);
```

Ping-pong 对象只负责选择 buffer，本身不调度 load、compute 和 store。

## Banked 和延迟 buffer

```cpp
ahls::mem::banked<word_t, 256, 4> banks;
banks.at(bank_id, row) = value;

ahls::mem::ring<word_t, 4> delay;
delay.push(value);
auto previous = delay.delay<1>();

ahls::mem::shift_register<word_t, 0, 1, 2> taps;
taps.Shift(value);
auto newest = taps.Get<2>();

ahls::mem::scratchpad<word_t, 256> scratch;
scratch.write(index, value);

ahls::mem::multi_buffer<ahls::mem::tile<word_t, 64>, 3> multi;
auto& slot = multi.slot_for(tile_id);

ahls::mem::triple_buffer<ahls::mem::tile<word_t, 64>> triple;
auto& load_tile = triple.load(tile_id);

ahls::mem::banked_tile<word_t, 256, 4> banked_tile;
banked_tile.set<0, 0>(value);
```

`banked<T, Depth, Banks>` 显式暴露 bank 维度。`ring` 提供编译期 delay 读取。
`shift_register` 里最大 tap index 对应最新 shift 进去的值。需要在综合中完全 partition
bank 维度时，在 kernel scope 内调用 `banks.partition()`。`banked_tile` 使用相同的
模板顺序 `banked_tile<T, Depth, Banks>`，内部存储仍是 `data[Banks][Depth]`。
`scratchpad`、
`multi_buffer`、`triple_buffer` 和 `banked_tile` 都只是显式本地存储的薄封装；
它们不暗示额外 memory port。需要并行访问时，用对应 partition helper 明确表达硬件结构。
`triple_buffer` 暴露三个 slot；stage 如何轮转由调用者传入的 tile id 决定，比如
`load(t)`、`compute(t - 1)`、`store(t - 2)`。如果给这些 stage helper 传入相同
`tile_id`，它们会有意返回同一个 slot。

## Line 和 window buffer

```cpp
ahls::mem::line_buffer<sample_t, 3, 1920> lines;
ahls::mem::window_buffer<sample_t, 3, 3> window;

lines.fill(0);
window.fill(0);
lines.shift_up(col, sample);
lines.shift_up<0>(sample);
window.shift_left(row, lines.at(row, col));
window.shift_left<1>(sample);
auto center = window.get<1, 1>();
```

`line_buffer` 表达按列索引的行历史。`shift_up(col, value)` 会把该列向 row 0
方向移动，并把最新值写入最后一行。`window_buffer` 表达完全本地的滑动窗口。
需要把这些维度变成显式并行硬件时，在 kernel 内调用 `partition_rows()` 或
`partition_complete()`。运行时索引 API（`at(row, col)`、`shift_up(col, ...)`、
`shift_left(row, ...)`）不做边界检查；索引是静态常量时优先用 `get<Row, Col>()`、
`set<Row, Col>()`、`shift_up<Col>()` 和 `shift_left<Row>()`。

## Double-buffered load-compute-store

```cpp
struct Pipeline {
  using input_t = const word_t*;
  using output_t = word_t*;
  using tile_t = ahls::mem::tile<word_t, 1024>;

  static void load(input_t in, int tile_id, tile_t& dst);
  static void compute(int tile_id, tile_t& src, tile_t& dst);
  static void store(output_t out, int tile_id, const tile_t& src);
};

ahls::dataflow::dbuf_lcs<Pipeline>(in, out, n_tiles);
```

`n_tiles` 是 tile 数量。当前第一版是 correctness skeleton：它显式选择 ping-pong
bank，但不宣称 load、compute、store 在硬件上已经重叠。

## Prefix scan、histogram 和 counting sort reorder

```cpp
int values[8];
int inclusive[8];
int exclusive[8];
ahls::compute::inclusive_scan<8>(values, inclusive);
ahls::compute::exclusive_scan<8>(values, exclusive);
ahls::compute::prefix_sum<8>(values, exclusive);  // exclusive prefix sum

ap_uint<2> keys[8];
ahls::compute::count_t<8> counts[4];
ahls::compute::histogram<2, 4>(keys, counts);

using fixed_key_t = ahls::ufx<8, 4>;
fixed_key_t fixed_keys[8];
struct FixedIntBin {
  static int bin(fixed_key_t value) { return static_cast<int>(value) & 3; }
};
ahls::compute::histogram<fixed_key_t, 8, 4, FixedIntBin>(fixed_keys, counts);

payload_t payloads[8];
ap_uint<2> sorted_keys[8];
payload_t sorted_payloads[8];
ahls::compute::counting_sort_reorder<2, 4>(
    keys, payloads, sorted_keys, sorted_payloads);
```

这些 scan 和 counting primitive 第一版是顺序、stable、保守可综合实现。
`prefix_sum` 和 `prefix_sum_inplace` 是 exclusive scan；inclusive 结果使用
`inclusive_scan` 或 `inclusive_scan_inplace`。histogram 和 scatter 阶段不会强行
`II=1`，因为多个元素写同一个 bin 时存在真实依赖。
`histogram_accumulate` 对 `counts[bin]` 有 read-modify-write 依赖，所以吞吐量和输入
冲突情况相关；有冲突 bin 时不应该期待达到每周期一个输入。
`count_t<N>` 是足够计数 `N` 个元素的 `ap_uint`。Histogram 可以使用小于完整 key
domain 的 bin 数，并忽略 `[0, Bins)` 之外的 key；`counting_sort_reorder` 要求
`Bins == 2^KeyBits`，保证每条输入记录都会被写出一次。当前 unrolled
histogram/counting-sort 路径限制在 256 个 bin；更大的 domain 应该用 radix pass
或后续 BRAM-backed histogram。out-of-place scan 对 in-place 调用是 alias-safe；
`counting_sort_reorder` 要求输入/输出数组不能 alias。

## 小规模排序和 top-k

```cpp
score_t scores[16];
ahls::compute::sort<16>(scores);
ahls::compute::sort<ahls::op::greater<score_t>, 16>(scores);

ap_uint<12> keys[64];
payload_t payloads[64];
ahls::compute::radix_sort_by_key<12, 4>(keys, payloads);

score_t best[4];
ahls::compute::topk<16, 4>(scores, best);
```

这些 API 是固定规模硬件结构，不是动态 STL 风格排序函数。`bitonic_sort` 要求 `N`
是 2 的幂；`sort` 和 `topk` 也继承这个限制。默认 `topk` 使用 `op::less`，所以返回
最小的 K 个值；如果要最大值优先，传入 `op::greater<T>`。`radix_sort` 和
`radix_sort_by_key` 对 unsigned `ap_uint<KeyBits>` key 排序，radix 宽度是编译期参数。
`radix_sort_by_key` 是 stable 的，所以相同 key 的 payload 顺序会保留。

## 组件 kernel

微架构原语的 component kernel 是 opt-in HLS 检查：

```bash
make csynth-component TARGET=u250 COMPONENT=tile_burst
make cosim-component TARGET=u250 COMPONENT=pingpong_dbuf_lcs
make csynth-component TARGET=u250 COMPONENT=radix_sort
make cosim-component TARGET=u250 COMPONENT=line_window
make csynth-component TARGET=u250 COMPONENT=prefix_histogram
make cosim-component TARGET=u250 COMPONENT=counting_sort_buffer
make analyze-component TARGET=u250 COMPONENT=tile_burst
```

当前组件包括 `tile_burst`、`pingpong_dbuf_lcs`、`radix_sort`、`line_window`、
`prefix_histogram` 和 `counting_sort_buffer`。
