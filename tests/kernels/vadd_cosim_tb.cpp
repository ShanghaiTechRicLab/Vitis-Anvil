#include <anvil/kernels/vadd.hpp>

#include <cstdio>
#include <vector>

using anvil::kernels::VaddPack;
using anvil::kernels::kVaddPack;

int main() {
    constexpr int kPacks = 16;
    constexpr int kInterfaceDepthPacks = 1024;
    std::vector<VaddPack> a(kInterfaceDepthPacks), b(kInterfaceDepthPacks), out(kInterfaceDepthPacks);
    for (int i = 0; i < kPacks; ++i) {
        for (int j = 0; j < kVaddPack; ++j) {
            a[i].Set(j, static_cast<float>(i * kVaddPack + j));
            b[i].Set(j, 1.0f);
        }
    }
    vadd(a.data(), b.data(), out.data(), kPacks);
    for (int i = 0; i < kPacks; ++i) {
        for (int j = 0; j < kVaddPack; ++j) {
            const float expect = static_cast<float>(i * kVaddPack + j + 1);
            if (out[i][j] != expect) {
                std::fprintf(stderr, "MISMATCH @ pack=%d lane=%d\n", i, j);
                return 1;
            }
        }
    }
    return 0;
}
