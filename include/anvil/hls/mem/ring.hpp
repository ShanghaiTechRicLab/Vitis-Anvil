#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int N>
struct ring {
  static_assert(N > 0, "ring: N must be positive");

  typedef T value_type;
  static const int size = N;

  T data[N];
  int head;

  ring() : data(), head(0) {}

  void push(T value) {
#pragma HLS inline
    head = (head == 0) ? (N - 1) : (head - 1);
    data[head] = value;
  }

  template <int Delay>
  T delay() const {
#pragma HLS inline
    static_assert(Delay >= 0, "ring::delay must be non-negative");
    static_assert(Delay < N, "ring::delay exceeds ring depth");
    return data[(head + Delay) % N];
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
