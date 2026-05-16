#pragma once
// Generic packed-memory and packed-stream helpers built on anvil::hls::Pack.
// C++14-clean and suitable for Vitis HLS kernels and host simulation tests.

#include "anvil/hls/pack.hpp"

namespace anvil {
namespace hls {

template <typename PackT, typename StreamT>
inline void LoadPacks(const PackT* in, StreamT& out, int n_packs) {
  for (int i = 0; i < n_packs; ++i) {
#pragma HLS pipeline II=1
    out.Push(in[i]);
  }
}

template <typename StreamT, typename PackT>
inline void StorePacks(StreamT& in, PackT* out, int n_packs) {
  for (int i = 0; i < n_packs; ++i) {
#pragma HLS pipeline II=1
    out[i] = in.Pop();
  }
}

template <typename PackT, typename StreamA, typename StreamB, typename StreamOut, typename Scalar, typename Op>
inline void MapPacksWithScalar(StreamA& a_stream,
                               StreamB& b_stream,
                               StreamOut& out_stream,
                               Scalar scalar,
                               int n_packs,
                               Op op) {
  for (int i = 0; i < n_packs; ++i) {
#pragma HLS pipeline II=1
    PackT a = a_stream.Pop();
    PackT b = b_stream.Pop();
    PackT out;
    for (int lane = 0; lane < PackTraits<PackT>::width; ++lane) {
#pragma HLS unroll
      SetLane(out, lane, op(scalar, GetLane(a, lane), GetLane(b, lane)));
    }
    out_stream.Push(out);
  }
}

template <typename PackT, typename StreamA, typename StreamB, typename StreamOut, typename Op>
inline void MapPacks(StreamA& a_stream,
                     StreamB& b_stream,
                     StreamOut& out_stream,
                     int n_packs,
                     Op op) {
  for (int i = 0; i < n_packs; ++i) {
#pragma HLS pipeline II=1
    PackT a = a_stream.Pop();
    PackT b = b_stream.Pop();
    PackT out;
    for (int lane = 0; lane < PackTraits<PackT>::width; ++lane) {
#pragma HLS unroll
      SetLane(out, lane, op(GetLane(a, lane), GetLane(b, lane)));
    }
    out_stream.Push(out);
  }
}

template <typename PackT, typename Op>
inline void MapMem2Packs(const PackT* a,
                         const PackT* b,
                         PackT* out,
                         int n_packs,
                         Op op) {
  for (int i = 0; i < n_packs; ++i) {
#pragma HLS pipeline II=1
    PackT pa = a[i];
    PackT pb = b[i];
    PackT po;
    for (int lane = 0; lane < PackTraits<PackT>::width; ++lane) {
#pragma HLS unroll
      SetLane(po, lane, op(GetLane(pa, lane), GetLane(pb, lane)));
    }
    out[i] = po;
  }
}

}  // namespace hls
}  // namespace anvil
