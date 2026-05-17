#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

int main() {
  using x_t = ahls::fx<16, 6>;
  using ux_t = ahls::ufx<8, 4>;
  using acc_t = ahls::acc_t<x_t, 8>;
  using uacc_t = ahls::acc_t<ux_t, 8>;

  static_assert(ahls::fixed_traits<acc_t>::is_signed, "signed accumulator");
  static_assert(!ahls::fixed_traits<uacc_t>::is_signed,
                "unsigned accumulator");

  ahls::mem::tile<ap_uint<32>, 4> tile{};
  ahls::mem::pingpong<ahls::mem::tile<ap_uint<32>, 4> > pp{};
  ahls::mem::banked<int, 4, 2> banks{};
  ahls::mem::scratchpad<int, 4> scratch{};
  ahls::mem::multi_buffer<ahls::mem::tile<int, 2>, 3> multi{};
  ahls::mem::triple_buffer<ahls::mem::tile<int, 2> > triple{};
  ahls::mem::banked_tile<int, 2, 4> banked_tile{};
  ahls::mem::line_buffer<int, 2, 3> lines{};
  ahls::mem::window_buffer<int, 2, 2> window{};
  ahls::mem::ring<int, 3> ring{};
  ahls::mem::shift_register<int, 0, 1> sr{};

  int values[4] = {3, 1, 2, 0};
  int top[2] = {};
  int scan_in[4] = {1, 2, 3, 4};
  int scan_out[4] = {};
  int prefix_out[4] = {};
  ap_uint<2> hist_keys[4] = {0, 1, 1, 3};
  ahls::compute::count_t<4> counts[4];
  ap_uint<2> sorted_keys[4] = {};
  int sorted_payloads[4] = {};
  ap_uint<8> keys[4] = {9, 1, 7, 3};
  ap_uint<4> payload_keys[4] = {2, 1, 2, 0};
  int payloads[4] = {20, 10, 21, 0};

  tile[0] = 1;
  pp.write(0)[0] = tile[0];
  banks.partition();
  banks.at(0, 0) = 1;
  scratch.fill(0);
  scratch.write(0, 1);
  multi.slot<0>()[0] = scratch.read(0);
  triple.load(0)[0] = multi.slot_for(0)[0];
  banked_tile.fill(0);
  banked_tile.set<0, 0>(triple.store(0)[0]);
  lines.fill(0);
  lines.shift_up(1, 2);
  lines.shift_up<2>(2);
  window.fill(0);
  window.shift_left(0, 3);
  window.shift_left<1>(3);
  ring.push(1);
  sr.Shift(1);
  (void)ahls::compute::tree_reduce<ahls::op::add<int> >(values);
  ahls::compute::inclusive_scan<4>(scan_in, scan_out);
  ahls::compute::prefix_sum<4>(scan_in, prefix_out);
  ahls::compute::prefix_sum_inplace<4>(scan_in);
  ahls::compute::histogram<2, 4>(hist_keys, counts);
  ahls::compute::counting_sort_reorder<2, 4>(
      hist_keys, scan_in, sorted_keys, sorted_payloads);
  ahls::compute::sort<4>(values);
  ahls::compute::sort<ahls::op::greater<int>, 4>(values);
  ahls::compute::radix_sort<8, 4>(keys);
  ahls::compute::radix_sort_by_key<4, 2>(payload_keys, payloads);
  ahls::compute::topk<4, 2>(values, top);

  return static_cast<int>(pp.read(0)[0]) + banks.at(0, 0) +
         banked_tile.get<0, 0>() + ring.delay<0>() + sr.Get<1>() + top[0] -
         5;
}
