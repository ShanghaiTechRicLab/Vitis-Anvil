#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace anvil::compare {

template <typename T>
double MaxAbsError(std::span<const T> a, std::span<const T> b) {
    if (a.size() != b.size()) throw std::invalid_argument("size mismatch");
    double m = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        m = std::max(m, std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i])));
    }
    return m;
}

template <typename Range>
double MaxAbsError(const Range& a, const Range& b) {
    using T = typename Range::value_type;
    return MaxAbsError<T>(std::span<const T>(a.data(), a.size()),
                          std::span<const T>(b.data(), b.size()));
}

template <typename T>
double RmsError(std::span<const T> a, std::span<const T> b) {
    if (a.size() != b.size()) throw std::invalid_argument("size mismatch");
    if (a.empty()) return 0.0;
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        s += d * d;
    }
    return std::sqrt(s / static_cast<double>(a.size()));
}

template <typename Range>
double RmsError(const Range& a, const Range& b) {
    using T = typename Range::value_type;
    return RmsError<T>(std::span<const T>(a.data(), a.size()),
                       std::span<const T>(b.data(), b.size()));
}

template <typename T>
double MeanAbsError(std::span<const T> a, std::span<const T> b) {
    if (a.size() != b.size()) throw std::invalid_argument("size mismatch");
    if (a.empty()) return 0.0;
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        s += std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
    }
    return s / static_cast<double>(a.size());
}


template <typename Range>
double MeanAbsError(const Range& a, const Range& b) {
    using T = typename Range::value_type;
    return MeanAbsError<T>(std::span<const T>(a.data(), a.size()),
                           std::span<const T>(b.data(), b.size()));
}

template <typename T>
double CosineSimilarity(std::span<const T> a, std::span<const T> b) {
    if (a.size() != b.size()) throw std::invalid_argument("size mismatch");
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        double ai = static_cast<double>(a[i]);
        double bi = static_cast<double>(b[i]);
        dot += ai * bi;
        na += ai * ai;
        nb += bi * bi;
    }
    double denom = std::sqrt(na) * std::sqrt(nb);
    return denom == 0.0 ? 0.0 : dot / denom;
}


template <typename Range>
double CosineSimilarity(const Range& a, const Range& b) {
    using T = typename Range::value_type;
    return CosineSimilarity<T>(std::span<const T>(a.data(), a.size()),
                               std::span<const T>(b.data(), b.size()));
}

}  // namespace anvil::compare
