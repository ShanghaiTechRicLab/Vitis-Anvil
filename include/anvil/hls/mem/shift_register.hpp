#pragma once

namespace anvil {
namespace hls {
namespace mem {

namespace detail {

template <int A, int B>
struct static_max2 {
  static const int value = (A < B) ? B : A;
};

template <int... Values>
struct static_max;

template <int Value>
struct static_max<Value> {
  static const int value = Value;
};

template <int First, int Second, int... Rest>
struct static_max<First, Second, Rest...> {
  static const int value =
      static_max2<First, static_max<Second, Rest...>::value>::value;
};

template <int... Values>
struct all_nonnegative;

template <>
struct all_nonnegative<> {
  static const bool value = true;
};

template <int First, int... Rest>
struct all_nonnegative<First, Rest...> {
  static const bool value = (First >= 0) && all_nonnegative<Rest...>::value;
};

}  // namespace detail

template <typename T, int... Taps>
class shift_register {
 public:
  static_assert(sizeof...(Taps) > 0,
                "shift_register: at least one tap is required");
  static_assert(detail::all_nonnegative<Taps...>::value,
                "shift_register: taps must be non-negative");

  static const int max_tap = detail::static_max<Taps...>::value;
  static const int size = max_tap + 1;

  shift_register() : data_() {}

  void Shift(const T& value) {
#pragma HLS inline
    for (int i = 0; i < max_tap; ++i) {
#pragma HLS unroll
      data_[i] = data_[i + 1];
    }
    data_[max_tap] = value;
  }

  template <int Tap>
  T Get() const {
#pragma HLS inline
    static_assert(Tap >= 0, "shift_register tap must be non-negative");
    static_assert(Tap <= max_tap, "shift_register tap exceeds max tap");
    return data_[Tap];
  }

 private:
  T data_[size];
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
