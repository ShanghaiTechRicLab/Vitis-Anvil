#pragma once

#include <anvil/hls/util.hpp>

#include <ap_fixed.h>

namespace anvil {
namespace hls {

template <int W, int I, ap_q_mode Q = AP_TRN, ap_o_mode O = AP_WRAP>
using fx = ap_fixed<W, I, Q, O>;

template <int W, int I, ap_q_mode Q = AP_TRN, ap_o_mode O = AP_WRAP>
using ufx = ap_ufixed<W, I, Q, O>;

template <typename T>
struct fixed_traits;

template <int W, int I, ap_q_mode Q, ap_o_mode O, int N>
struct fixed_traits<ap_fixed<W, I, Q, O, N> > {
  static const int width = W;
  static const int integer = I;
  static const int fractional = W - I;
  static const bool is_signed = true;
  static const ap_q_mode q_mode = Q;
  static const ap_o_mode o_mode = O;
};

template <int W, int I, ap_q_mode Q, ap_o_mode O, int N>
struct fixed_traits<ap_ufixed<W, I, Q, O, N> > {
  static const int width = W;
  static const int integer = I;
  static const int fractional = W - I;
  static const bool is_signed = false;
  static const ap_q_mode q_mode = Q;
  static const ap_o_mode o_mode = O;
};

namespace detail {

template <typename T, int Count, int GuardBits, bool Signed>
struct acc_type;

template <typename T, int Count, int GuardBits>
struct acc_type<T, Count, GuardBits, true> {
  static_assert(Count > 0, "acc_t: Count must be positive");
  static_assert(GuardBits >= 0, "acc_t: GuardBits must be non-negative");

  typedef ap_fixed<
      fixed_traits<T>::width + util::ceil_log2(Count) + GuardBits,
      fixed_traits<T>::integer + util::ceil_log2(Count) + GuardBits>
      type;
};

template <typename T, int Count, int GuardBits>
struct acc_type<T, Count, GuardBits, false> {
  static_assert(Count > 0, "acc_t: Count must be positive");
  static_assert(GuardBits >= 0, "acc_t: GuardBits must be non-negative");

  typedef ap_ufixed<
      fixed_traits<T>::width + util::ceil_log2(Count) + GuardBits,
      fixed_traits<T>::integer + util::ceil_log2(Count) + GuardBits>
      type;
};

template <typename To, bool Signed>
struct saturating_type;

template <typename To>
struct saturating_type<To, true> {
  typedef ap_fixed<fixed_traits<To>::width,
                   fixed_traits<To>::integer,
                   fixed_traits<To>::q_mode,
                   AP_SAT>
      type;
};

template <typename To>
struct saturating_type<To, false> {
  typedef ap_ufixed<fixed_traits<To>::width,
                    fixed_traits<To>::integer,
                    fixed_traits<To>::q_mode,
                    AP_SAT>
      type;
};

}  // namespace detail

template <typename T, int Count, int GuardBits = 4>
using acc_t =
    typename detail::acc_type<T, Count, GuardBits,
                              fixed_traits<T>::is_signed>::type;

template <typename To, typename From>
To saturate_cast(From value) {
#pragma HLS inline
  typedef typename detail::saturating_type<
      To, fixed_traits<To>::is_signed>::type sat_to_t;
  return static_cast<To>(sat_to_t(value));
}

}  // namespace hls
}  // namespace anvil
