#pragma once
#include <algorithm>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace anvil::compare {

inline double Top1Accuracy(std::span<const int> labels,
                           std::span<const float> logits, int n_class) {
    if (labels.empty()) return 0.0;
    if (n_class <= 0) throw std::invalid_argument("n_class must be positive");
    if (logits.size() != labels.size() * static_cast<std::size_t>(n_class)) {
        throw std::invalid_argument("logits size must be labels.size() * n_class");
    }
    std::size_t correct = 0;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const float* row = logits.data() + i * n_class;
        int pred = static_cast<int>(std::max_element(row, row + n_class) - row);
        if (pred == labels[i]) ++correct;
    }
    return static_cast<double>(correct) / static_cast<double>(labels.size());
}

inline double TopKAccuracy(std::span<const int> labels,
                           std::span<const float> logits, int n_class, int k) {
    if (labels.empty()) return 0.0;
    if (n_class <= 0 || k <= 0 || k > n_class) throw std::invalid_argument("invalid n_class/k");
    if (logits.size() != labels.size() * static_cast<std::size_t>(n_class)) {
        throw std::invalid_argument("logits size must be labels.size() * n_class");
    }
    std::size_t correct = 0;
    std::vector<int> idx(n_class);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const float* row = logits.data() + i * n_class;
        std::iota(idx.begin(), idx.end(), 0);
        std::partial_sort(idx.begin(), idx.begin() + k, idx.end(),
                          [row](int a, int b) { return row[a] > row[b]; });
        for (int j = 0; j < k; ++j) {
            if (idx[j] == labels[i]) { ++correct; break; }
        }
    }
    return static_cast<double>(correct) / static_cast<double>(labels.size());
}

}  // namespace anvil::compare
