#pragma once
#include <anvil/compare/element_wise.hpp>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>

namespace anvil::compare {

inline double Psnr(std::span<const float> ref, std::span<const float> test, float peak) {
    double rms = RmsError<float>(ref, test);
    if (rms == 0.0) return std::numeric_limits<double>::infinity();
    return 20.0 * std::log10(static_cast<double>(peak) / rms);
}

inline double Snr(std::span<const float> ref, std::span<const float> test) {
    if (ref.size() != test.size()) throw std::invalid_argument("size mismatch");
    double sig_power = 0.0, noise_power = 0.0;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        double r = ref[i], d = ref[i] - test[i];
        sig_power += r * r;
        noise_power += d * d;
    }
    if (noise_power == 0.0) return std::numeric_limits<double>::infinity();
    return 10.0 * std::log10(sig_power / noise_power);
}

}  // namespace anvil::compare
