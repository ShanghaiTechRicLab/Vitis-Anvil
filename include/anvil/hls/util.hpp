#pragma once

namespace anvil {
namespace hls {
namespace util {

constexpr int ceil_div(int a, int b) {
  return (a + b - 1) / b;
}

constexpr int ceil_log2_impl(int value, int power, int log) {
  return power >= value ? log : ceil_log2_impl(value, power << 1, log + 1);
}

constexpr int ceil_log2(int value) {
  return value <= 1 ? 0 : ceil_log2_impl(value, 1, 0);
}

}  // namespace util
}  // namespace hls
}  // namespace anvil
