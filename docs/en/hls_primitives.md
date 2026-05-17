# HLS architecture primitives

Vitis-Anvil provides small HLS architecture primitives under `anvil::hls`.
These helpers are not an algorithm library. They expose common hardware
structures used inside kernels: fixed-point types, tile buffers, banked buffers,
ring buffers, shift registers, line/window buffers, burst load/store,
ping-pong buffers, reductions, small fixed-size sorters, radix sort, top-k
selectors, and double-buffered load-compute-store skeletons.

## Include style

```cpp
#include <anvil/hls.hpp>
namespace ahls = anvil::hls;
```

## Fixed point

```cpp
using x_t = ahls::fx<16, 6>;
using acc_t = ahls::acc_t<x_t, 256>;
```

`acc_t<T, N>` widens the accumulator by `ceil_log2(N)` plus guard bits.

## Tile and burst

```cpp
using word_t = ap_uint<512>;
ahls::mem::tile<word_t, 1024> tile;

ahls::mem::load_burst<16>(in, base, tile);
ahls::mem::store_burst<16>(out, base, tile);
```

`base` is measured in `word_t` elements, not bytes. The burst length is visible
in the template argument so it can become part of synthesis action metadata and
future loop-shaping policy.

## Ping-pong

```cpp
ahls::mem::pingpong<ahls::mem::tile<word_t, 1024>> pp;
auto& load_tile = pp.write(tile_id);
auto& compute_tile = pp.read(tile_id - 1);
```

The ping-pong object selects buffers. It does not schedule load, compute, and
store by itself.

## Banked and delay buffers

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

`banked<T, Depth, Banks>` exposes the bank dimension explicitly. `ring` provides
compile-time delay reads. `shift_register` uses the largest tap index for the
newest shifted value. Call `banks.partition()` inside the kernel scope when the
bank dimension should be completely partitioned for synthesis.

## Line and window buffers

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

`line_buffer` models row history indexed by column. `shift_up(col, value)`
moves one column toward row zero and writes the newest value into the last row.
`window_buffer` models a fully local sliding window. Call `partition_rows()` or
`partition_complete()` inside the kernel when those dimensions should be
explicit hardware parallelism. Runtime-index APIs (`at(row, col)`,
`shift_up(col, ...)`, `shift_left(row, ...)`) are unchecked; prefer templated
variants such as `get<Row, Col>()`, `set<Row, Col>()`, `shift_up<Col>()`, and
`shift_left<Row>()` when the index is static.

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

`n_tiles` is a tile count. The first implementation is a correctness skeleton:
it makes ping-pong bank selection explicit but does not claim hardware overlap
between load, compute, and store.

## Small sorting and top-k

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

These APIs are fixed-size hardware structures. They are not dynamic STL-style
sort functions. `sort`, `bitonic_sort`, and `topk` currently require a
power-of-two `N`. Default `topk` uses `op::less`, so it returns the smallest
values first; pass `op::greater<T>` for largest-first ordering. `radix_sort`
and `radix_sort_by_key` sort unsigned `ap_uint<KeyBits>` keys with compile-time
radix width. `radix_sort_by_key` is stable, so equal keys preserve payload
order.

## Component kernels

Primitive component kernels are opt-in HLS checks:

```bash
make csynth-component TARGET=u250 COMPONENT=tile_burst
make cosim-component TARGET=u250 COMPONENT=pingpong_dbuf_lcs
make csynth-component TARGET=u250 COMPONENT=radix_sort
make cosim-component TARGET=u250 COMPONENT=line_window
make analyze-component TARGET=u250 COMPONENT=tile_burst
```

Available components are `tile_burst`, `pingpong_dbuf_lcs`, `radix_sort`, and
`line_window`.
