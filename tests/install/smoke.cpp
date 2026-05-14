#include <anvil/compare/element_wise.hpp>

#include <cstdio>
#include <span>
#include <vector>

int main() {
    std::vector<float> a{1.0F, 2.0F, 3.0F};
    std::vector<float> b{1.0F, 2.0F, 3.0F};
    const double err = anvil::compare::MaxAbsError(std::span<const float>(a), std::span<const float>(b));
    std::printf("install_smoke: MaxAbsError = %g\n", err);
    return err == 0.0 ? 0 : 1;
}
