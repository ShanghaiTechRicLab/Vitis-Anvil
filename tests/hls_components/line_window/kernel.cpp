#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

extern "C" void component_line_window(const ap_uint<16>* in,
                                       ap_uint<16>* out) {
#pragma HLS interface m_axi port=in bundle=gmem0 depth=16
#pragma HLS interface m_axi port=out bundle=gmem1 depth=16
#pragma HLS interface s_axilite port=return bundle=control

  ahls::mem::line_buffer<ap_uint<16>, 2, 4> lines;
  ahls::mem::window_buffer<ap_uint<16>, 2, 2> window;
  lines.partition_rows();
  window.partition_complete();
  lines.fill(0);
  window.fill(0);

  for (int i = 0; i < 8; ++i) {
#pragma HLS pipeline II=1
    const int col = i & 3;
    lines.shift_up(col, in[i]);
    window.shift_left<0>(lines.at(1, col));
    window.shift_left<1>(lines.at(0, col));
    out[i] = window.get<0, 1>() + window.get<1, 1>();
  }
}
