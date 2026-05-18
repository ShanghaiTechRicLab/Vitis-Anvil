# HLS Architecture Primitives

## What is this?

`anvil::hls` is a small C++ header-only library shipped with Vitis-Anvil that
targets **Vitis HLS / Xilinx FPGA kernels**. It is **not** an algorithm
library — it packages the hardware structures you keep rewriting in HLS
kernels (tiles, ping-pong buffers, line buffers, scans, histograms, sorters)
into reusable, synthesizable building blocks with static parameter checks.

Every primitive shares the same contract:

- **Header-only.** `#include <anvil/hls.hpp>` pulls in the whole set.
- **C++14.** Compiles under Vitis HLS (`v++`, `vitis_hls`) and host g++, so
  the same code can power both the kernel and a software model used for
  co-sim / golden-vector checks.
- **Zero runtime allocation.** Every buffer is a stack-allocated `T data[N]`
  that HLS can directly map to BRAM / URAM / FIFO / registers.
- **Pragmas baked in.** Primitives carry their own `#pragma HLS inline`,
  `pipeline`, `unroll`, and `array_partition` directives so callers rarely
  need to add more.
- **Static checks everywhere.** All template parameters (`N`, `Bins`,
  `Depth`, `KeyBits`, …) are validated with `static_assert`, so shape
  mistakes fail at compile time rather than after a long synthesis run.

## Feature overview

| Group                  | Primitives                                                                                  |
| ---------------------- | ------------------------------------------------------------------------------------------- |
| Fixed point            | `fx`, `ufx`, `fixed_traits`, `acc_t`, `saturate_cast`                                       |
| Meta operators         | `op::add`, `op::less`, `op::greater`                                                        |
| Tile + burst           | `mem::tile`, `mem::load_burst`, `mem::store_burst`                                          |
| Multi-buffering        | `mem::pingpong`, `mem::multi_buffer`, `mem::triple_buffer`                                  |
| Banked storage         | `mem::banked`, `mem::banked_tile`                                                           |
| Local storage          | `mem::scratchpad`                                                                           |
| Delay lines            | `mem::ring`, `mem::shift_register`                                                          |
| Line / window buffers  | `mem::line_buffer`, `mem::window_buffer`                                                    |
| Reductions and scans   | `compute::reduce`, `tree_reduce`, `sum`, `dot`, `inclusive_scan`, `exclusive_scan`          |
| Histogram + bucket sort| `compute::histogram`, `histogram_accumulate`, `count_t`, `counting_sort_reorder`            |
| Sort and top-k         | `compute::sort`, `bitonic_sort`, `radix_sort`, `radix_sort_by_key`, `topk`                  |
| Dataflow               | `dataflow::dbuf_lcs`                                                                        |

## How do I use it?

```cpp
#include <anvil/hls.hpp>            // pulls in every primitive
namespace ahls = anvil::hls;        // recommended alias
```

For a finer-grained include, reach for the leaf header directly (for example
`<anvil/hls/mem/tile.hpp>`); no algorithm-level dependencies are dragged in.

## Where does it fit?

- Convolution / pooling / accumulators: use `acc_t` for accumulator widths
  and `tree_reduce` for parallel sums.
- Tile-streaming kernels: combine `tile` + `pingpong` + `dbuf_lcs` to
  scaffold load-compute-store pipelines.
- Stencil / image filters: keep N rows in `line_buffer`, then materialise an
  NxN neighbourhood inside a `window_buffer`.
- Sort / top-k: power-of-two N → `bitonic_sort` / `topk`; `ap_uint` keys
  with arbitrary N → `radix_sort`; small key domain → `counting_sort_reorder`.
- Histogram, prefix sums, counters: drop in the matching primitive instead
  of rewriting the same `II=1` loop pattern by hand.

---

# Fixed point (`anvil/hls/fixed.hpp`)

## `fx<W, I>` / `ufx<W, I>`

**What it is.** Aliases for `ap_fixed` / `ap_ufixed` with sensible default
quantisation/overflow modes (`AP_TRN`, `AP_WRAP`).

**What it gives you.** A drop-in replacement for `float` / `double` inside
kernels that saves DSPs and shortens timing paths.

**Where it fits.** DSP pipelines, quantised inference, filter coefficients
and samples.

```cpp
// Signed fixed: 16 bits total, 6 integer bits (incl. sign), 10 fractional bits
using x_t  = ahls::fx<16, 6>;
// Unsigned fixed: 8 bits total, 4 integer bits, 4 fractional bits
using ux_t = ahls::ufx<8, 4>;
```

## `fixed_traits<T>`

**What it is.** A trait probe that exposes `width`, `integer`, `fractional`,
`is_signed`, `q_mode`, `o_mode` as compile-time constants for any
`ap_fixed` / `ap_ufixed` type.

**What it gives you.** A way to write generic HLS templates that derive
their intermediate types from the input element type.

```cpp
// Compile-time check that x_t is the signed variant
static_assert(ahls::fixed_traits<x_t>::is_signed, "x_t should be signed");
// Pull out the fractional width for generic code
constexpr int frac = ahls::fixed_traits<x_t>::fractional;  // == 10
```

## `acc_t<T, Count, GuardBits = 4>`

**What it is.** An accumulator type derived from `T` whose integer width is
widened by `ceil_log2(Count) + GuardBits`, preserving the signedness of `T`.

**What it gives you.** No more eyeballed accumulator widths — wide enough
to sum `Count` items of `T` without overflow, narrow enough not to waste
DSPs.

**Where it fits.** MACs, in-tile reductions, integral images, any
fixed-iteration accumulation.

```cpp
using x_t   = ahls::fx<16, 6>;
// Grow by ceil_log2(256) + 4 = +12 bits, so 256 adds of x_t can't overflow.
using acc_t = ahls::acc_t<x_t, 256>;

acc_t acc = 0;
for (int i = 0; i < 256; ++i) {
#pragma HLS pipeline II=1
  acc += static_cast<acc_t>(samples[i]);  // safe even at worst-case values
}
```

## `saturate_cast<To>(value)`

**What it is.** A cast that clamps `value` into the representable range of
`To` using `AP_SAT` instead of wrapping.

**What it gives you.** Safe narrowing from a wide accumulator back to an
output type without silent wrap-around bugs.

```cpp
using out_t = ahls::fx<8, 2>;
// Anything outside out_t's range is clamped to ±max, not wrapped.
out_t y = ahls::saturate_cast<out_t>(acc);
```

---

# Tile + burst (`anvil/hls/mem/tile.hpp`, `burst.hpp`)

## `mem::tile<T, N>`

**What it is.** A thin wrapper over `T data[N]` with `operator[]`.

**What it gives you.** A named unit of "local data tile" that pairs with
`load_burst` / `store_burst` to ferry data between DDR and BRAM.

**Where it fits.** Every tile-based kernel — GEMM, conv, vector ops where
work is partitioned into tiles.

```cpp
using word_t = ap_uint<512>;                  // 512-bit AXI burst word
ahls::mem::tile<word_t, 1024> tile;           // 1024 words = 64 KiB local buffer
tile[0] = 1;                                  // standard indexing
```

## `mem::load_burst<BurstLen>(in, base, dst)` / `store_burst<BurstLen>`

**What it is.** Sequentially copy `N` elements from `in + base` into `dst`
(or the reverse) at `II=1`. `BurstLen` is a template parameter that
surfaces the requested burst length for synthesis-action metadata and
future loop-shaping policy.

**What it gives you.** Loops the HLS compiler can infer into AXI bursts
without you having to retype the same II=1 pattern.

**Where it fits.** The load and store stages of every `dbuf_lcs` pipeline.

**Watch out.** `base` is measured in **elements**, not bytes.

```cpp
ahls::mem::tile<word_t, 1024> tile;
// Burst-read 1024 words starting at element offset tile_id * 1024.
ahls::mem::load_burst<16>(in_ptr,  tile_id * 1024, tile);
// ... compute against `tile` ...
// Burst-write the same span back out.
ahls::mem::store_burst<16>(out_ptr, tile_id * 1024, tile);
```

---

# Multi-buffering (`pingpong.hpp`, `multi_buffer.hpp`, `triple_buffer.hpp`)

## `mem::pingpong<TileT>`

**What it is.** Two `TileT` slots selected by `tile_id & 1`. `read(t)` and
`write(t)` map the same `t` to the same slot; the caller alternates by
passing different tile IDs.

**What it gives you.** A clear separation between the buffer being loaded
and the one being consumed.

**Where it fits.** Boundary between load↔compute or compute↔store stages.

```cpp
ahls::mem::pingpong<ahls::mem::tile<word_t, 1024>> pp;
auto& load_into    = pp.write(t);     // current tile lands here
auto& consume_from = pp.read(t - 1);  // previous tile is read from the other slot
```

## `mem::multi_buffer<TileT, Slots>`

**What it is.** Generalisation of `pingpong` to `Slots = 2/3/4/…`.
`slot_for(tile_id)` returns the `tile_id mod Slots` slot (correct for
negatives too).

**What it gives you.** A small fixed-size buffer pool for deeper pipelines
(e.g. prefetch + compute + flush).

```cpp
ahls::mem::multi_buffer<ahls::mem::tile<int, 64>, 4> mb;
auto& s  = mb.slot_for(tile_id);   // dynamic 4-slot rotation
auto& s0 = mb.slot<0>();           // static slot pick (range-checked)
```

## `mem::triple_buffer<TileT>`

**What it is.** A wrapper over `multi_buffer<TileT, 3>` that exposes the
`load(t)` / `compute(t)` / `store(t)` role aliases (**all three call the
same `slot_for`**).

**What it gives you.** A vocabulary for three-stage pipelines —
**rotation is the caller's job** via different tile IDs, the canonical use
being `load(t)`, `compute(t-1)`, `store(t-2)`.

**Gotcha.** Passing the same `tile_id` to all three stages intentionally
returns the same slot (handy when stages collapse).

```cpp
ahls::mem::triple_buffer<ahls::mem::tile<int, 64>> tb;
for (int t = 0; t < n_tiles + 2; ++t) {
  // Load stage: write tile t into slot t % 3.
  if (t < n_tiles)              load_stage   (tb.load(t),       t);
  // Compute stage: read tile t-1 from slot (t-1) % 3.
  if (t >= 1 && t <= n_tiles)   compute_stage(tb.compute(t-1),  t-1);
  // Store stage: read tile t-2 from slot (t-2) % 3.
  if (t >= 2)                   store_stage  (tb.store(t-2),    t-2);
}
```

---

# Banked storage (`banked.hpp`, `banked_tile.hpp`)

## `mem::banked<T, Depth, Banks>`

**What it is.** A wrapper over `T data[Banks][Depth]` exposing
`at(bank, idx)` and a one-liner `partition()` that fully partitions the
bank dimension.

**What it gives you.** Multi-port BRAMs without typing the partition pragma
yourself; parallel access to several banks per cycle.

**Where it fits.** Systolic-array weight/activation tiles, vectorised
reduce staging buffers.

```cpp
ahls::mem::banked<word_t, 256, 4> banks;   // 4 banks × 256 deep
banks.partition();                          // fully partition the bank dim (1 port per bank)
banks.at(bank_id, row) = value;             // write into a specific bank
auto v = banks.at(bank_id, row);            // and read back
```

## `mem::banked_tile<T, Depth, Banks>`

**What it is.** Same `[Banks][Depth]` storage as `banked` but with
tile-flavoured affordances: `get<Bank,Idx>()` / `set<Bank,Idx>()` /
`fill()`.

**What it gives you.** A `tile`-equivalent local buffer that wants parallel
multi-bank access.

**Watch out.** Parameter order is `<T, Depth, Banks>`, matching `banked`;
the underlying layout is `data[Banks][Depth]`.

```cpp
ahls::mem::banked_tile<int, 256, 4> bt;     // 4 banks × 256 elements
bt.partition_banks();                        // partition the bank dim
bt.fill(0);                                  // zero every element
bt.set<0, 0>(42);                            // compile-time index → zero-cost path
int x = bt.get<0, 0>();                      // ditto for the read
```

---

# Plain local storage (`scratchpad.hpp`)

## `mem::scratchpad<T, Depth>`

**What it is.** A single-port 1D local array with explicit
`read/write/at/get/set/fill` APIs; `partition_complete()` partitions every
element.

**What it gives you.** A scratch buffer whose intent reads more clearly
than a bare `T arr[Depth]` when banking isn't needed.

```cpp
ahls::mem::scratchpad<word_t, 256> sp;
sp.fill(0);                       // II=1 init loop
sp.write(i, v);                   // dynamic-index write
v = sp.read(i);                   // dynamic-index read
sp.set<7>(v);                     // compile-time-index write (static_assert range)
auto x = sp.get<7>();             // ditto for read
```

---

# Delay lines (`ring.hpp`, `shift_register.hpp`)

## `mem::ring<T, N>`

**What it is.** N-slot circular buffer. `push(v)` advances the head and
stores; `delay<D>()` reads the value from `D` cycles ago (compile-time
indexed, out-of-range fails to compile).

**What it gives you.** A clean way to keep the last N values without
hand-rolling head-pointer arithmetic.

**Where it fits.** FIR tap delays, IIR state, any short look-back logic.

```cpp
ahls::mem::ring<int, 4> r;
r.push(x);                  // make x the newest value
int now    = r.delay<0>();  // value pushed this cycle
int prev   = r.delay<1>();  // 1 cycle ago
int oldest = r.delay<3>();  // 3 cycles ago; delay<4> fails to compile
```

## `mem::shift_register<T, Taps...>`

**What it is.** A shift register with a fixed set of tap positions.
`Shift(v)` shifts everything down by one and inserts at the largest tap;
`Get<Tap>()` reads a specific tap.

**What it gives you.** More intent than `ring` when only specific tap
positions are read (FIR/IIR/conv).

```cpp
ahls::mem::shift_register<int, 0, 1, 2> sr;  // taps at positions 0, 1, 2
sr.Shift(x);                                  // x lands at tap 2 (newest)
int t0 = sr.Get<0>();                         // oldest tap
int t2 = sr.Get<2>();                         // newest (the x we just shifted in)
```

---

# Line history + sliding window (`line_buffer.hpp`, `window_buffer.hpp`)

## `mem::line_buffer<T, Rows, Cols>`

**What it is.** A `Rows × Cols` row history. `shift_up(col, v)` shifts that
column toward row 0, dropping the oldest row and writing the new value into
the bottom row.

**What it gives you.** The classic stencil row cache: keep the most recent
`Rows-1` rows plus the row currently being read.

**Where it fits.** 3x3 / 5x5 convolutions, Sobel, Gaussian, morphology row
buffers.

```cpp
ahls::mem::line_buffer<sample_t, 3, 1920> lines;  // 3 rows × 1920 cols (HD width)
lines.partition_rows();                            // expose 3 rows as parallel ports
lines.fill(0);

for (int col = 0; col < 1920; ++col) {
#pragma HLS pipeline II=1
  sample_t new_pixel = read_input();
  // For this column: row0 ← row1, row1 ← row2, row2 ← new_pixel.
  lines.shift_up(col, new_pixel);
  sample_t r0 = lines.at(0, col);   // two-row-old sample
  sample_t r1 = lines.at(1, col);   // one-row-old sample
  sample_t r2 = lines.at(2, col);   // current sample (== new_pixel)
}
```

## `mem::window_buffer<T, Rows, Cols>`

**What it is.** A fully-local `Rows × Cols` sliding window with parallel
access to every element. Comes with `shift_left(row, v)` (shift row left,
new value enters the rightmost column), `shift_up(col, v)`, and templated
counterparts.

**What it gives you.** Paired with `line_buffer`, the NxN neighbourhood
your stencil kernel actually reads from every cycle.

```cpp
ahls::mem::line_buffer<sample_t, 3, 1920> lines;
ahls::mem::window_buffer<sample_t, 3, 3>   win;
lines.partition_rows();
win.partition_complete();   // keep all 9 cells in registers

for (int col = 0; col < 1920; ++col) {
#pragma HLS pipeline II=1
  sample_t p = read_input();
  lines.shift_up(col, p);          // update row cache for this column
  // Slide the 3x3 window one column to the left,
  // then pull the freshest 3 rows from the line buffer into the right edge.
  for (int r = 0; r < 3; ++r) {
#pragma HLS unroll
    win.shift_left(r, lines.at(r, col));
  }
  sample_t center = win.get<1, 1>();   // compile-time access to the window center
}
```

---

# Reductions (`compute/reduce.hpp`)

## `compute::reduce<Op>(values)`

**What it is.** Sequential reduction `acc = Op::apply(acc, v)` over N
values (O(N) depth).

**What it gives you.** A short, generic reducer that accepts any `Op`
satisfying the trait (`add`, custom `max`, custom `min`, …).

```cpp
int v[8];
int s = ahls::compute::reduce<ahls::op::add<int>>(v);   // = v[0] + ... + v[7]
```

## `compute::tree_reduce<Op>(values)`

**What it is.** Tree reduction with O(log N) depth. Works for **any** N — a
trailing element at an odd level passes through unchanged, so no data is
lost when N is not a power of two.

**What it gives you.** Short timing depth even for wide reductions; the
go-to when sequential reduce becomes the critical path.

```cpp
int v[5];   // 5 is not a power of two — still fine
int s = ahls::compute::tree_reduce<ahls::op::add<int>>(v);
```

## `compute::sum<N, AccT>(values)` / `dot<N, AccT>(x, w)`

**What it is.** Shorthand for the two most common reductions. `sum` adds;
`dot` accumulates `x[i] * w[i]`. The caller picks `AccT`, ideally
`acc_t<...>`.

```cpp
using acc_t = ahls::acc_t<ahls::fx<16, 6>, 64>;
acc_t sum_v = ahls::compute::sum<64, acc_t>(values);   // 64-term sum
acc_t dotxy = ahls::compute::dot<64, acc_t>(x, w);     // ⟨x, w⟩
```

---

# Prefix scans (`compute/prefix_sum.hpp`)

## `inclusive_scan<N>(in, out)` / `exclusive_scan<N>(in, out)`

**What it is.** Inclusive / exclusive prefix sums. Inclusive:
`out[i] = in[0..i]`. Exclusive: `out[i] = in[0..i-1]`, `out[0] = 0`.

**What it gives you.** The two building blocks behind histogram-to-offsets
conversion and flag-to-write-position stream compaction.

**Alias-safe.** The loop reads `in[i]` **before** writing `out[i]`, so you
may pass the same array as both input and output for an in-place scan.
Convenience aliases `prefix_sum_inplace` / `inclusive_scan_inplace` exist
for that case.

```cpp
int in[8]  = {1, 2, 3, 4, 5, 6, 7, 8};
int inc[8], exc[8];
ahls::compute::inclusive_scan<8>(in, inc);   // 1, 3, 6, 10, 15, 21, 28, 36
ahls::compute::exclusive_scan<8>(in, exc);   // 0, 1, 3, 6, 10, 15, 21, 28
ahls::compute::prefix_sum_inplace<8>(in);    // in now holds the exclusive scan
```

## `prefix_sum<N>(in, out)`

**What it is.** Alias for `exclusive_scan`, named after the common
parallel-computing convention.

---

# Histogram (`compute/histogram.hpp`)

## `count_t<N>`

**What it is.** The narrowest `ap_uint` that can count up to `N` items
(`ceil_log2(N+1)` bits).

**What it gives you.** Right-sized bin counters without wasting `int`
width.

## `histogram_reset<Bins>(counts)`

**What it is.** Zeroes `counts[0..Bins)` with an unrolled loop.

## `histogram_accumulate<KeyBits, Bins>(keys, counts)`

**What it is.** Treats `keys[i]` as a bin index and increments
`counts[bin]`, ignoring `bin >= Bins`.

**Performance note.** `counts[bin]` is a **read-modify-write** dependency,
so HLS cannot achieve a steady `II=1`; throughput drops when keys collide
on the same bin. For higher throughput, switch to a radix pass or a
two-stage bucket-then-merge structure.

## `histogram<KeyBits, Bins>(keys, counts)` (reset + accumulate combined)

```cpp
ap_uint<2> keys[8] = {0, 1, 1, 3, 2, 0, 1, 0};
ahls::compute::count_t<8> counts[4];               // 4 bins, max count 8 → 4-bit counter
ahls::compute::histogram<2, 4>(keys, counts);      // counts = {3, 3, 1, 1}
```

## `histogram<KeyT, N, NumBins, BinPolicy>(keys, bins)` (custom binning)

**What it gives you.** Histogramming over non-`ap_uint` keys (e.g.
`ap_fixed`, custom records) by routing each key through a user policy.

```cpp
using key_t = ahls::ufx<8, 4>;
struct LowBitsBin {
  // Drop everything but the low 2 bits to derive the bin index.
  static int bin(key_t v) { return static_cast<int>(v) & 3; }
};
key_t keys[8];
ahls::compute::count_t<8> bins[4];
ahls::compute::histogram<key_t, 8, 4, LowBitsBin>(keys, bins);
```

**Limits.** `Bins ≤ 256` (unroll/partition budget); `KeyBits ≤ 30` (the
`int` bin index must not overflow). Larger bin counts will need a future
BRAM-backed histogram.

---

# Counting sort (`compute/counting_sort.hpp`)

## `counting_sort_reorder<KeyBits, Bins, N, PayloadT>(keys, payloads, out_keys, out_payloads)`

**What it is.** Stable full-domain counting sort: histogram → exclusive
scan to per-bin start offsets → left-to-right scatter that emits both keys
and payloads.

**What it gives you.** Stable O(N) sorting when the key domain fits in
256 bins — cheaper than bitonic for small key widths.

**Constraints.** `Bins == 2^KeyBits` (full domain required so every record
gets written exactly once); input and output arrays **must not alias**.

**Where it fits.** Lowest-radix pass, bucket sort, grouping records by
class.

```cpp
ap_uint<2> keys[8]     = {3, 1, 2, 0, 1, 3, 0, 1};
int        payloads[8] = {30, 10, 20, 0, 11, 31, 1, 12};
ap_uint<2> ok[8];
int        op[8];
ahls::compute::counting_sort_reorder<2, 4>(keys, payloads, ok, op);
// ok = {0, 0, 1, 1, 1, 2, 3, 3}
// payloads for equal keys keep their input order (stability).
```

---

# Sort and top-k (`compute/sort.hpp`, `topk.hpp`, `radix_sort.hpp`)

## `compute::bitonic_sort<N, T, Compare>(values)` / `sort<N>` / `sort<Compare, N>`

**What it is.** A fully-unrolled fixed-size bitonic sort with O(log²N)
compare depth.

**What it gives you.** Balanced area-vs-timing parallel sort for small N
(roughly up to 128 elements).

**Constraint.** N must be a **power of two** (`static_assert` enforced).

```cpp
int v[16];
ahls::compute::sort<16>(v);                                 // ascending (default op::less)
ahls::compute::sort<ahls::op::greater<int>, 16>(v);         // descending
```

## `compute::topk<N, K>(in, out)` / `topk<Compare, N, K>`

**What it is.** Bitonic-sort the entire `in[N]`, then copy the first K
elements to `out`.

**What it gives you.** Top-K selection for classifier outputs, ranked
candidates, etc.

**Constraints.** N is a power of two; `K ≤ N`. Default `op::less` returns
the smallest K; pass `op::greater<T>` for the largest K.

```cpp
int scores[16];
int best4[4];
ahls::compute::topk<16, 4>(scores, best4);                          // 4 smallest
ahls::compute::topk<ahls::op::greater<int>, 16, 4>(scores, best4);  // 4 largest
```

## `compute::radix_sort<KeyBits, RadixBits, N>(keys)` / `radix_sort_by_key<...>(keys, payloads)`

**What it is.** LSD radix sort. Each pass runs histogram → exclusive scan
→ left-to-right scatter; the by-key variant is stable.

**What it gives you.** Sorting when N is not a power of two, keys are
`ap_uint<KeyBits>`, and keys are wider than what `counting_sort_reorder`
can do directly.

**Constraints.** `0 < RadixBits ≤ KeyBits`; `RadixBits ≤ 8` (histograms
capped at 256 bins). Throughput bound by the same histogram RAW hazard
(not `II=1`).

```cpp
ap_uint<12> keys[64];
int         payloads[64];
// 12-bit keys, 4-bit radix → ceil(12/4) = 3 passes, stable by-key sort.
ahls::compute::radix_sort_by_key<12, 4>(keys, payloads);
ahls::compute::radix_sort<8, 4>(keys8);   // keys-only variant
```

---

# Meta operators (`anvil/hls/op.hpp`)

| Type             | Methods                              | Used by               |
| ---------------- | ------------------------------------ | --------------------- |
| `op::add<T>`     | `identity()`, `apply(a, b) -> a + b` | reduce, tree_reduce   |
| `op::less<T>`    | `before(a, b) -> a < b`              | sort, topk            |
| `op::greater<T>` | `before(a, b) -> a > b`              | sort, topk descending |

You can plug in your own `struct` satisfying the same protocol (`op::max`,
`op::xor_`, …) and pass it to the same primitives.

---

# Double-buffered LCS (`dataflow/dbuf_lcs.hpp`)

## `dataflow::dbuf_lcs<Policy>(in, out, n_tiles)`

**What it is.** A skeleton for the load-compute-store three-stage pipeline.
`Policy` is a user struct that defines the three stage callbacks; the
function keeps two `pingpong<tile_t>` instances to decouple input from
output.

**What it gives you.** A canonical place to put boilerplate so the kernel
body focuses on per-stage logic.

**Status.** Today this is a **correctness skeleton**. Ping-pong banks are
selected correctly, but no `pragma HLS dataflow` or loop pipeline is
asserted between stages — that's deliberate so the function does not
over-promise hardware overlap that the surrounding code may not deliver.
A future iteration will move it to a true dataflow implementation.

```cpp
struct MyPipeline {
  using input_t  = const word_t*;
  using output_t = word_t*;
  using tile_t   = ahls::mem::tile<word_t, 1024>;

  // DDR -> local tile (use load_burst inside).
  static void load   (input_t  in,  int t, tile_t& dst);
  // Local compute on a tile (src -> dst).
  static void compute(int t, tile_t& src, tile_t& dst);
  // Local tile -> DDR (use store_burst inside).
  static void store  (output_t out, int t, const tile_t& src);
};

ahls::dataflow::dbuf_lcs<MyPipeline>(in_ptr, out_ptr, n_tiles);
```

---

# Subset includes

Use just the mem layer:

```cpp
#include <anvil/hls/mem/tile.hpp>
#include <anvil/hls/mem/burst.hpp>
#include <anvil/hls/mem/pingpong.hpp>
```

Use just the compute layer:

```cpp
#include <anvil/hls/compute/reduce.hpp>
#include <anvil/hls/compute/sort.hpp>
```

`<anvil/hls.hpp>` is the one-stop entry point that drags in everything.

---

# Component kernels (synthesis smoke tests)

Each primitive ships a minimal HLS kernel + cosim testbench under
`tests/hls_components/<name>/`. Run them individually to check synthesis
quality:

```bash
make csynth-component  TARGET=u250 COMPONENT=tile_burst
make cosim-component   TARGET=u250 COMPONENT=pingpong_dbuf_lcs
make csynth-component  TARGET=u250 COMPONENT=radix_sort
make cosim-component   TARGET=u250 COMPONENT=line_window
make csynth-component  TARGET=u250 COMPONENT=prefix_histogram
make cosim-component   TARGET=u250 COMPONENT=counting_sort_buffer
make analyze-component TARGET=u250 COMPONENT=tile_burst
```

Available `COMPONENT` values: `tile_burst`, `pingpong_dbuf_lcs`,
`radix_sort`, `line_window`, `prefix_histogram`, `counting_sort_buffer`.
These are **not tutorial examples** — they are the minimal synthesizable
samples that run in CI, but they double as known-good templates when you
get stuck on a real kernel.

For a pure host-side compile check (no Vitis / XRT / xpfm / sysroot
needed), build and run `tests/cpp/test_anvil_hls_primitives_cxx14.cpp` —
it instantiates every public primitive under C++14 and exits with a
non-trivial result.
