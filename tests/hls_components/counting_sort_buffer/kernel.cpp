#include <anvil/hls.hpp>

#include <ap_int.h>

namespace ahls = anvil::hls;

extern "C" void component_counting_sort_buffer(const ap_uint<2>* in_keys,
                                                const ap_uint<16>* in_payloads,
                                                ap_uint<2>* out_keys,
                                                ap_uint<16>* out_payloads) {
#pragma HLS interface m_axi port=in_keys bundle=gmem0 depth=8
#pragma HLS interface m_axi port=in_payloads bundle=gmem1 depth=8
#pragma HLS interface m_axi port=out_keys bundle=gmem2 depth=8
#pragma HLS interface m_axi port=out_payloads bundle=gmem3 depth=8
#pragma HLS interface s_axilite port=return bundle=control

  static const int N = 8;
  static const int Bins = 4;

  ahls::mem::scratchpad<ap_uint<2>, N> key_pad;
  ahls::mem::scratchpad<ap_uint<16>, N> payload_pad;
  ap_uint<2> keys[N];
  ap_uint<16> payloads[N];
  ap_uint<2> sorted_keys[N];
  ap_uint<16> sorted_payloads[N];
  ahls::mem::multi_buffer<ahls::mem::tile<ap_uint<16>, 4>, 2> mb;
  ahls::mem::triple_buffer<ahls::mem::tile<ap_uint<16>, 4> > tb;
  ahls::mem::banked_tile<ap_uint<16>, 4, 2> banks;
#pragma HLS array_partition variable=keys complete
#pragma HLS array_partition variable=payloads complete
#pragma HLS array_partition variable=sorted_keys complete
#pragma HLS array_partition variable=sorted_payloads complete
#pragma HLS array_partition variable=mb.buffers complete dim=0
#pragma HLS array_partition variable=tb.storage.buffers complete dim=0
#pragma HLS array_partition variable=banks.data complete dim=0

  key_pad.partition_complete();
  payload_pad.partition_complete();
  banks.partition_banks();

  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    key_pad.write(i, in_keys[i]);
    payload_pad.write(i, in_payloads[i]);
  }

  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    keys[i] = key_pad.read(i);
    payloads[i] = payload_pad.read(i);
  }

  ahls::compute::counting_sort_reorder<2, Bins>(
      keys, payloads, sorted_keys, sorted_payloads);

  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    mb.slot<0>()[i] = sorted_payloads[i];
    mb.slot<1>()[i] = sorted_payloads[i + 4];
  }

  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    tb.load(0)[i] = mb.slot<0>()[i];
    tb.compute(1)[i] = mb.slot<1>()[i];
    banks.at(0, i) = tb.store(0)[i];
    banks.at(1, i) = tb.store(1)[i];
  }

  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    out_keys[i] = sorted_keys[i];
    out_payloads[i] = (i < 4) ? banks.at(0, i) : banks.at(1, i - 4);
  }
}
