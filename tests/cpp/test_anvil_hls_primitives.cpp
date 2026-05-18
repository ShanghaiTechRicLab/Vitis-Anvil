#include <catch_amalgamated.hpp>

#include <anvil/hls.hpp>
#include <anvil/hls/compute/counting_sort.hpp>
#include <anvil/hls/compute/histogram.hpp>
#include <anvil/hls/compute/prefix_sum.hpp>
#include <anvil/hls/compute/reduce.hpp>
#include <anvil/hls/compute/sort.hpp>
#include <anvil/hls/compute/topk.hpp>
#include <anvil/hls/dataflow/dbuf_lcs.hpp>
#include <anvil/hls/fixed.hpp>
#include <anvil/hls/mem/banked.hpp>
#include <anvil/hls/mem/banked_tile.hpp>
#include <anvil/hls/mem/burst.hpp>
#include <anvil/hls/mem/line_buffer.hpp>
#include <anvil/hls/mem/multi_buffer.hpp>
#include <anvil/hls/mem/pingpong.hpp>
#include <anvil/hls/mem/ring.hpp>
#include <anvil/hls/mem/scratchpad.hpp>
#include <anvil/hls/mem/shift_register.hpp>
#include <anvil/hls/mem/tile.hpp>
#include <anvil/hls/mem/triple_buffer.hpp>
#include <anvil/hls/mem/window_buffer.hpp>
#include <anvil/hls/op.hpp>
#include <anvil/hls/util.hpp>

TEST_CASE("ceil_log2 computes integer bit growth", "[hls][util]") {
  static_assert(anvil::hls::util::ceil_log2(1) == 0, "ceil_log2(1)");
  static_assert(anvil::hls::util::ceil_log2(2) == 1, "ceil_log2(2)");
  static_assert(anvil::hls::util::ceil_log2(3) == 2, "ceil_log2(3)");
  static_assert(anvil::hls::util::ceil_log2(256) == 8, "ceil_log2(256)");
  REQUIRE(anvil::hls::util::ceil_log2(9) == 4);
}

TEST_CASE("fixed traits expose width and integer bits", "[hls][fixed]") {
  using x_t = anvil::hls::fx<16, 6>;
  using acc_t = anvil::hls::acc_t<x_t, 256>;

  static_assert(anvil::hls::fixed_traits<x_t>::width == 16, "width");
  static_assert(anvil::hls::fixed_traits<x_t>::integer == 6, "integer");
  static_assert(anvil::hls::fixed_traits<x_t>::fractional == 10,
                "fractional");
  static_assert(anvil::hls::fixed_traits<acc_t>::width == 28, "acc width");
  static_assert(anvil::hls::fixed_traits<acc_t>::integer == 18,
                "acc integer");

  x_t x = 1.25;
  acc_t y = anvil::hls::saturate_cast<acc_t>(x);
  REQUIRE(static_cast<double>(y) > 1.24);
  REQUIRE(static_cast<double>(y) < 1.26);
}

TEST_CASE("unsigned fixed accumulators remain unsigned", "[hls][fixed]") {
  using x_t = anvil::hls::ufx<8, 4>;
  using acc_t = anvil::hls::acc_t<x_t, 16>;

  static_assert(!anvil::hls::fixed_traits<x_t>::is_signed, "input unsigned");
  static_assert(!anvil::hls::fixed_traits<acc_t>::is_signed,
                "accumulator unsigned");
  static_assert(anvil::hls::fixed_traits<acc_t>::width == 16, "acc width");
  static_assert(anvil::hls::fixed_traits<acc_t>::integer == 12,
                "acc integer");
}

TEST_CASE("saturate_cast clamps fixed-point overflow", "[hls][fixed]") {
  using wide_t = anvil::hls::fx<8, 5>;
  using narrow_t = anvil::hls::fx<4, 2>;

  narrow_t high = anvil::hls::saturate_cast<narrow_t>(wide_t(7.0));
  narrow_t low = anvil::hls::saturate_cast<narrow_t>(wide_t(-7.0));

  REQUIRE(static_cast<double>(high) == Catch::Approx(1.75));
  REQUIRE(static_cast<double>(low) == Catch::Approx(-2.0));
}

TEST_CASE("operator policies work with scalar types", "[hls][op]") {
  REQUIRE(anvil::hls::op::add<int>::identity() == 0);
  REQUIRE(anvil::hls::op::add<int>::apply(2, 3) == 5);
  REQUIRE(anvil::hls::op::less<int>::before(1, 2));
  REQUIRE(anvil::hls::op::greater<int>::before(2, 1));
}

TEST_CASE("tile supports indexed access", "[hls][mem]") {
  anvil::hls::mem::tile<int, 4> t{};
  t[0] = 3;
  t[1] = 4;
  REQUIRE(t[0] == 3);
  REQUIRE(t[1] == 4);
  REQUIRE(t.size == 4);
}

TEST_CASE("load_burst and store_burst copy tile data", "[hls][mem]") {
  int in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  int out[8] = {};
  anvil::hls::mem::tile<int, 4> t{};

  anvil::hls::mem::load_burst<2>(in, 2, t);
  REQUIRE(t[0] == 2);
  REQUIRE(t[3] == 5);

  anvil::hls::mem::store_burst<2>(out, 1, t);
  REQUIRE(out[1] == 2);
  REQUIRE(out[4] == 5);
}

TEST_CASE("pingpong selects buffers by tile id", "[hls][mem]") {
  using tile_t = anvil::hls::mem::tile<int, 2>;
  anvil::hls::mem::pingpong<tile_t> pp{};

  pp.write(0)[0] = 10;
  pp.write(1)[0] = 20;

  REQUIRE(pp.read(0)[0] == 10);
  REQUIRE(pp.read(2)[0] == 10);
  REQUIRE(pp.read(1)[0] == 20);
  REQUIRE(pp.read(3)[0] == 20);
}

TEST_CASE("banked exposes independent banks", "[hls][mem]") {
  anvil::hls::mem::banked<int, 4, 2> banks{};
  banks.partition();
  banks.at(0, 1) = 11;
  banks.at(1, 1) = 21;

  REQUIRE(banks.at(0, 1) == 11);
  REQUIRE(banks.at(1, 1) == 21);
  REQUIRE(banks.bank(1)[1] == 21);
  REQUIRE(banks.depth == 4);
  REQUIRE(banks.banks == 2);
}

TEST_CASE("ring returns fixed compile-time delays", "[hls][mem]") {
  anvil::hls::mem::ring<int, 3> r{};
  r.push(10);
  r.push(20);
  r.push(30);

  REQUIRE(r.delay<0>() == 30);
  REQUIRE(r.delay<1>() == 20);
  REQUIRE(r.delay<2>() == 10);

  r.push(40);
  REQUIRE(r.delay<0>() == 40);
  REQUIRE(r.delay<2>() == 20);
}

TEST_CASE("shift_register exposes newest value at largest tap", "[hls][mem]") {
  anvil::hls::mem::shift_register<int, 0, 1> sr{};
  sr.Shift(10);
  REQUIRE(sr.Get<1>() == 10);
  sr.Shift(20);
  REQUIRE(sr.Get<0>() == 10);
  REQUIRE(sr.Get<1>() == 20);
}

TEST_CASE("line_buffer shifts columns toward row zero", "[hls][mem]") {
  anvil::hls::mem::line_buffer<int, 3, 2> lb{};
  lb.at(0, 1) = 11;
  lb.at(1, 1) = 21;
  lb.at(2, 1) = 31;

  lb.shift_up(1, 41);

  REQUIRE(lb.at(0, 1) == 21);
  REQUIRE(lb.at(1, 1) == 31);
  REQUIRE(lb.at(2, 1) == 41);
  lb.fill(-1);
  REQUIRE(lb.at(0, 0) == -1);
  REQUIRE(lb.at(2, 1) == -1);
  lb.set<1, 0>(55);
  REQUIRE(lb.get<1, 0>() == 55);
  lb.shift_up<0>(66);
  REQUIRE(lb.get<2, 0>() == 66);
  REQUIRE(lb.rows == 3);
  REQUIRE(lb.cols == 2);
}

TEST_CASE("window_buffer shifts rows left and exposes taps", "[hls][mem]") {
  anvil::hls::mem::window_buffer<int, 2, 3> win{};
  win.at(0, 0) = 1;
  win.at(0, 1) = 2;
  win.at(0, 2) = 3;

  win.shift_left(0, 4);

  REQUIRE(win.at(0, 0) == 2);
  REQUIRE(win.at(0, 1) == 3);
  REQUIRE(win.at(0, 2) == 4);
  REQUIRE(win.get<0, 2>() == 4);
  win.fill(-2);
  REQUIRE(win.at(0, 0) == -2);
  REQUIRE(win.at(1, 2) == -2);
  win.set<1, 0>(7);
  REQUIRE(win.get<1, 0>() == 7);
  win.shift_left<1>(8);
  REQUIRE(win.get<1, 1>() == -2);
  REQUIRE(win.get<1, 2>() == 8);
  win.shift_up<2>(9);
  REQUIRE(win.get<1, 2>() == 9);
  REQUIRE(win.rows == 2);
  REQUIRE(win.cols == 3);
}

TEST_CASE("scratchpad exposes local SRAM-style storage", "[hls][mem]") {
  anvil::hls::mem::scratchpad<int, 4> sp{};
  sp.fill(7);
  REQUIRE(sp.read(0) == 7);
  sp.write(2, 9);
  REQUIRE(sp.read(2) == 9);
  sp.set<1>(5);
  REQUIRE(sp.get<1>() == 5);
  REQUIRE(sp.depth == 4);
}

TEST_CASE("multi_buffer selects reusable slots by id", "[hls][mem]") {
  typedef anvil::hls::mem::tile<int, 2> tile_t;
  anvil::hls::mem::multi_buffer<tile_t, 3> mb{};
  mb.slot(0)[0] = 10;
  mb.slot(1)[0] = 20;
  mb.slot(2)[0] = 30;

  REQUIRE(mb.slot_for(3)[0] == 10);
  REQUIRE(mb.slot_for(-1)[0] == 30);
  REQUIRE(mb.slot<1>()[0] == 20);
  REQUIRE(mb.slots == 3);
}

TEST_CASE("triple_buffer aliases stages by tile id", "[hls][mem]") {
  typedef anvil::hls::mem::tile<int, 1> tile_t;
  anvil::hls::mem::triple_buffer<tile_t> tb{};
  tb.load(0)[0] = 10;
  tb.compute(1)[0] = 20;
  tb.store(2)[0] = 30;

  REQUIRE(tb.load(3)[0] == 10);
  REQUIRE(tb.compute(4)[0] == 20);
  REQUIRE(tb.store(5)[0] == 30);
  REQUIRE(tb.slots == 3);
}

TEST_CASE("banked_tile combines bank and tile indexing", "[hls][mem]") {
  anvil::hls::mem::banked_tile<int, 4, 2> bt{};
  bt.fill(0);
  bt.at(0, 1) = 11;
  bt.set<1, 2>(22);

  REQUIRE(bt.at(0, 1) == 11);
  REQUIRE(bt.get<1, 2>() == 22);
  REQUIRE(bt.bank(1)[2] == 22);
  REQUIRE(bt.banks == 2);
  REQUIRE(bt.depth == 4);
}

TEST_CASE("sum and dot expose accumulator type", "[hls][compute]") {
  using x_t = anvil::hls::fx<16, 6>;
  using acc_t = anvil::hls::acc_t<x_t, 4>;

  x_t xs[4] = {1.0, 2.0, 3.0, 4.0};
  x_t ws[4] = {2.0, 2.0, 2.0, 2.0};

  acc_t sum = anvil::hls::compute::sum<4, acc_t>(xs);
  acc_t dot = anvil::hls::compute::dot<4, acc_t>(xs, ws);

  REQUIRE(static_cast<double>(sum) == Catch::Approx(10.0));
  REQUIRE(static_cast<double>(dot) == Catch::Approx(20.0));
}

TEST_CASE("reduce accepts operator policy first", "[hls][compute]") {
  int values[4] = {1, 2, 3, 4};
  int reduced = anvil::hls::compute::reduce<anvil::hls::op::add<int> >(values);
  REQUIRE(reduced == 10);
}

TEST_CASE("tree_reduce combines values with logarithmic structure", "[hls][compute]") {
  int values[5] = {1, 2, 3, 4, 5};
  int reduced =
      anvil::hls::compute::tree_reduce<anvil::hls::op::add<int> >(values);
  REQUIRE(reduced == 15);
}

TEST_CASE("prefix sum exposes inclusive exclusive and inplace scans", "[hls][compute]") {
  int values[5] = {2, 1, 3, 0, 4};
  int alias_values[5] = {2, 1, 3, 0, 4};
  int inclusive_values[5] = {2, 1, 3, 0, 4};
  int inclusive[5] = {};
  int exclusive[5] = {};
  int prefix[5] = {};

  anvil::hls::compute::inclusive_scan<5>(values, inclusive);
  anvil::hls::compute::exclusive_scan<5>(values, exclusive);
  anvil::hls::compute::prefix_sum<5>(values, prefix);
  anvil::hls::compute::prefix_sum<5>(alias_values, alias_values);
  anvil::hls::compute::prefix_sum_inplace<5>(values);
  anvil::hls::compute::inclusive_scan_inplace<5>(inclusive_values);

  const int expected_inclusive[5] = {2, 3, 6, 6, 10};
  const int expected_exclusive[5] = {0, 2, 3, 6, 6};
  for (int i = 0; i < 5; ++i) {
    REQUIRE(inclusive[i] == expected_inclusive[i]);
    REQUIRE(exclusive[i] == expected_exclusive[i]);
    REQUIRE(prefix[i] == expected_exclusive[i]);
    REQUIRE(alias_values[i] == expected_exclusive[i]);
    REQUIRE(values[i] == expected_exclusive[i]);
    REQUIRE(inclusive_values[i] == expected_inclusive[i]);
  }
}

namespace {

struct LowTwoBitsBinPolicy {
  static int bin(unsigned value) { return static_cast<int>(value & 3u); }
};

}  // namespace

TEST_CASE("generic histogram accepts bin policy", "[hls][compute]") {
  unsigned values[6] = {0, 1, 5, 2, 7, 4};
  anvil::hls::compute::count_t<6> bins[4];

  anvil::hls::compute::histogram<unsigned, 6, 4, LowTwoBitsBinPolicy>(
      values, bins);

  const unsigned expected[4] = {2, 2, 1, 1};
  for (int i = 0; i < 4; ++i) {
    REQUIRE(bins[i] == expected[i]);
  }
}

TEST_CASE("histogram counts ap_uint keys into bounded counters", "[hls][compute]") {
  ap_uint<2> keys[8] = {0, 1, 3, 1, 2, 3, 3, 0};
  anvil::hls::compute::count_t<8> counts[4];

  anvil::hls::compute::histogram<2, 4>(keys, counts);

  const int expected[4] = {2, 2, 1, 3};
  for (int i = 0; i < 4; ++i) {
    REQUIRE(static_cast<unsigned>(counts[i]) == expected[i]);
  }
}

TEST_CASE("counting_sort_reorder stably reorders payloads", "[hls][compute]") {
  ap_uint<2> keys[8] = {2, 1, 2, 0, 1, 3, 0, 2};
  int payloads[8] = {20, 10, 21, 0, 11, 30, 1, 22};
  ap_uint<2> out_keys[8] = {};
  int out_payloads[8] = {};

  anvil::hls::compute::counting_sort_reorder<2, 4>(
      keys, payloads, out_keys, out_payloads);

  const int expected_keys[8] = {0, 0, 1, 1, 2, 2, 2, 3};
  const int expected_payloads[8] = {0, 1, 10, 11, 20, 21, 22, 30};
  for (int i = 0; i < 8; ++i) {
    REQUIRE(static_cast<unsigned>(out_keys[i]) == expected_keys[i]);
    REQUIRE(out_payloads[i] == expected_payloads[i]);
  }
}

TEST_CASE("sort orders fixed-size arrays", "[hls][compute]") {
  int values[8] = {7, 3, 5, 1, 6, 2, 4, 0};
  anvil::hls::compute::sort<8>(values);
  for (int i = 0; i < 8; ++i) {
    REQUIRE(values[i] == i);
  }
}

TEST_CASE("sort accepts comparator policy first", "[hls][compute]") {
  int values[8] = {7, 3, 5, 1, 6, 2, 4, 0};
  anvil::hls::compute::sort<anvil::hls::op::greater<int>, 8>(values);
  for (int i = 0; i < 8; ++i) {
    REQUIRE(values[i] == 7 - i);
  }
}

TEST_CASE("radix_sort orders unsigned fixed-width keys", "[hls][compute]") {
  ap_uint<8> values[7] = {42, 3, 255, 0, 17, 3, 128};
  anvil::hls::compute::radix_sort<8, 4>(values);

  const int expected[7] = {0, 3, 3, 17, 42, 128, 255};
  for (int i = 0; i < 7; ++i) {
    REQUIRE(static_cast<unsigned>(values[i]) == expected[i]);
  }
}

TEST_CASE("radix_sort_by_key keeps payloads stable", "[hls][compute]") {
  ap_uint<4> keys[6] = {2, 1, 2, 0, 1, 3};
  int payloads[6] = {20, 10, 21, 0, 11, 30};

  anvil::hls::compute::radix_sort_by_key<4, 2>(keys, payloads);

  const int expected_keys[6] = {0, 1, 1, 2, 2, 3};
  const int expected_payloads[6] = {0, 10, 11, 20, 21, 30};
  for (int i = 0; i < 6; ++i) {
    REQUIRE(static_cast<unsigned>(keys[i]) == expected_keys[i]);
    REQUIRE(payloads[i] == expected_payloads[i]);
  }
}

TEST_CASE("topk returns smallest K values by default", "[hls][compute]") {
  int values[8] = {9, 1, 7, 3, 8, 2, 6, 4};
  int best[3] = {};
  anvil::hls::compute::topk<8, 3>(values, best);
  REQUIRE(best[0] == 1);
  REQUIRE(best[1] == 2);
  REQUIRE(best[2] == 3);
}

TEST_CASE("topk accepts custom comparator first", "[hls][compute]") {
  int values[8] = {9, 1, 7, 3, 8, 2, 6, 4};
  int best[2] = {};
  anvil::hls::compute::topk<anvil::hls::op::greater<int>, 8, 2>(values, best);
  REQUIRE(best[0] == 9);
  REQUIRE(best[1] == 8);
}

namespace {

struct AddOneDbufPolicy {
  typedef const int* input_t;
  typedef int* output_t;
  typedef anvil::hls::mem::tile<int, 4> tile_t;
  static const int TileSize = tile_t::size;

  static void load(input_t in, int tile_id, tile_t& dst) {
    anvil::hls::mem::load_burst<2>(in, tile_id * TileSize, dst);
  }

  static void compute(int tile_id, tile_t& src, tile_t& dst) {
    (void)tile_id;
    for (int i = 0; i < TileSize; ++i) {
      dst[i] = src[i] + 1;
    }
  }

  static void store(output_t out, int tile_id, const tile_t& src) {
    anvil::hls::mem::store_burst<2>(out, tile_id * TileSize, src);
  }
};

}  // namespace

TEST_CASE("dbuf_lcs applies policy over tile count", "[hls][dataflow]") {
  int in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  int out[8] = {};

  anvil::hls::dataflow::dbuf_lcs<AddOneDbufPolicy>(in, out, 2);

  for (int i = 0; i < 8; ++i) {
    REQUIRE(out[i] == in[i] + 1);
  }
}
