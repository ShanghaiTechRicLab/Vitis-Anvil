#pragma once
// Canonical Vitis-Anvil wrapper over hlslib Stream for internal dataflow.

#ifdef HLSLIB_SYNTHESIS
#include <hls_stream.h>
#else
#include <hlslib/xilinx/Stream.h>
#endif

namespace anvil {
namespace hls {

// Backward-compatible default: hls_aliases.hpp historically exposed
// Stream<T> as hlslib::Stream<T, 2>. Do not silently change implicit-depth
// call sites.
static const int kDefaultStreamDepth = 2;

// Recommended explicit depth for demo dataflow kernels/models.
static const int kDefaultDataflowStreamDepth = 32;

template <typename T, int Depth = kDefaultStreamDepth>
#ifdef HLSLIB_SYNTHESIS
class Stream {
 public:
  Stream() {
#pragma HLS INLINE
  }

  explicit Stream(const char*) {
#pragma HLS INLINE
#pragma HLS STREAM variable=stream_ depth=Depth
  }

  Stream(const Stream&) = delete;
  Stream(Stream&&) = delete;
  Stream& operator=(const Stream&) = delete;
  Stream& operator=(Stream&&) = delete;

  void Push(const T& value) {
#pragma HLS INLINE
    stream_.write(value);
  }

  T Pop() {
#pragma HLS INLINE
    return stream_.read();
  }

  void write(const T& value) {
#pragma HLS INLINE
    stream_.write(value);
  }

  T read() {
#pragma HLS INLINE
    return stream_.read();
  }

  void read(T& out) {
#pragma HLS INLINE
    out = stream_.read();
  }

  bool write_nb(const T& value) {
#pragma HLS INLINE
    return stream_.write_nb(value);
  }

  bool read_nb(T& out) {
#pragma HLS INLINE
    return stream_.read_nb(out);
  }

  bool empty() const {
#pragma HLS INLINE
    return stream_.empty();
  }

  bool full() const {
#pragma HLS INLINE
    return stream_.full();
  }

 private:
  ::hls::stream<T> stream_;
};
#else
using Stream = ::hlslib::Stream<T, Depth>;
#endif

}  // namespace hls
}  // namespace anvil
