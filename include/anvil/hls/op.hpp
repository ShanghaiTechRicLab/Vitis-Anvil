#pragma once

namespace anvil {
namespace hls {
namespace op {

template <typename T>
struct add {
  static T identity() { return T(0); }

  static T apply(T a, T b) {
#pragma HLS inline
    return a + b;
  }
};

template <typename T>
struct less {
  static bool before(const T& a, const T& b) {
#pragma HLS inline
    return a < b;
  }
};

template <typename T>
struct greater {
  static bool before(const T& a, const T& b) {
#pragma HLS inline
    return a > b;
  }
};

}  // namespace op
}  // namespace hls
}  // namespace anvil
