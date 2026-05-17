# HLS 微架构原语

Vitis-Anvil 在 `anvil::hls` 下提供小型 HLS 微架构原语。这不是算法库，而是 kernel
内部常用硬件结构：定点数、tile buffer、banked buffer、ring buffer、shift register、
burst load/store、ping-pong buffer、归约、小规模固定排序、top-k 选择，以及
double-buffered load-compute-store 骨架。

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
```

`banked<T, Depth, Banks>` 显式暴露 bank 维度。`ring` 提供编译期 delay 读取。
`shift_register` 里最大 tap index 对应最新 shift 进去的值。需要在综合中完全 partition
bank 维度时，在 kernel scope 内调用 `banks.partition()`。

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

## 小规模排序和 top-k

```cpp
score_t scores[16];
ahls::compute::sort<16>(scores);

score_t best[4];
ahls::compute::topk<16, 4>(scores, best);
```

这些 API 是固定规模硬件结构，不是动态 STL 风格排序函数。`bitonic_sort` 要求 `N`
是 2 的幂；`sort` 和 `topk` 也继承这个限制。默认 `topk` 使用 `op::less`，所以返回
最小的 K 个值；如果要最大值优先，传入 `op::greater<T>`。

## 组件 kernel

微架构原语的 component kernel 是 opt-in HLS 检查：

```bash
make csynth-component TARGET=u250 COMPONENT=tile_burst
make cosim-component TARGET=u250 COMPONENT=pingpong_dbuf_lcs
make analyze-component TARGET=u250 COMPONENT=tile_burst
```

第一批组件包括 `tile_burst` 和 `pingpong_dbuf_lcs`。
