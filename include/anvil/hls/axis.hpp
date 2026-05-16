#pragma once
// Minimal helpers for hls::stream-style AXI stream objects. hls::stream and
// hlslib::Stream both expose read()/write(), so one constrained overload per
// direction is enough.

namespace anvil {
namespace hls {

template <typename StreamT, typename T>
inline auto WriteAxis(StreamT& stream, const T& value) -> decltype(stream.write(value), void()) {
  stream.write(value);
}

template <typename StreamT>
inline auto ReadAxis(StreamT& stream) -> decltype(stream.read()) {
  return stream.read();
}

}  // namespace hls
}  // namespace anvil
