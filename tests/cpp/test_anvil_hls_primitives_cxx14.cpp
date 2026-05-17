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
  ahls::mem::ring<int, 3> ring{};
  ahls::mem::shift_register<int, 0, 1> sr{};

  int values[4] = {3, 1, 2, 0};
  int top[2] = {};

  tile[0] = 1;
  pp.write(0)[0] = tile[0];
  banks.partition();
  banks.at(0, 0) = 1;
  ring.push(1);
  sr.Shift(1);
  ahls::compute::sort<4>(values);
  ahls::compute::topk<4, 2>(values, top);

  return static_cast<int>(pp.read(0)[0]) + banks.at(0, 0) + ring.delay<0>() +
         sr.Get<1>() + top[0] - 4;
}
