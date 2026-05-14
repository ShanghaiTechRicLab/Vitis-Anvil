#pragma once
#include <bit>
#include <cstdint>
#include <limits>
#include <cstring>
#include <span>
#include <type_traits>

namespace anvil::compare {

template <typename T>
bool BitExact(std::span<const T> a, std::span<const T> b) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "BitExact requires trivially copyable element type");
    if (a.size() != b.size()) return false;
    return std::memcmp(a.data(), b.data(), a.size_bytes()) == 0;
}

template <typename Range>
bool BitExact(const Range& a, const Range& b) {
    using T = typename Range::value_type;
    return BitExact<T>(std::span<const T>(a.data(), a.size()),
                       std::span<const T>(b.data(), b.size()));
}

inline std::uint32_t UlpDiff(float a, float b) {
    if (a == b) return 0u;  // treats +0/-0 and equal finite values as 0 ULP apart
    auto ordered = [](float v) -> std::int32_t {
        std::int32_t i = std::bit_cast<std::int32_t>(v);
        return i < 0 ? std::numeric_limits<std::int32_t>::min() - i : i;
    };
    std::int64_t ia = static_cast<std::int64_t>(ordered(a));
    std::int64_t ib = static_cast<std::int64_t>(ordered(b));
    auto diff = ia > ib ? ia - ib : ib - ia;
    return static_cast<std::uint32_t>(diff);
}

}  // namespace anvil::compare
