#include <anvil/kernels/vadd.hpp>

#include <cstdio>
#include <vector>

using anvil::kernels::VaddPack;
using anvil::kernels::kVaddPack;

namespace {

constexpr int kPacks = 16;
constexpr int kInterfaceDepthPacks = 1024;
constexpr int kTransactions = 2;

int RunTransaction(int tx) {
    std::vector<VaddPack> a(kInterfaceDepthPacks), b(kInterfaceDepthPacks), out(kInterfaceDepthPacks);
    for (int i = 0; i < kPacks; ++i) {
        for (int j = 0; j < kVaddPack; ++j) {
            a[i].Set(j, static_cast<float>(tx * 1000 + i * kVaddPack + j));
            b[i].Set(j, static_cast<float>(1 + tx));
            out[i].Set(j, 0.0f);
        }
    }
    vadd(a.data(), b.data(), out.data(), kPacks);
    for (int i = 0; i < kPacks; ++i) {
        for (int j = 0; j < kVaddPack; ++j) {
            const float expect = static_cast<float>(tx * 1000 + i * kVaddPack + j + 1 + tx);
            if (out[i][j] != expect) {
                std::fprintf(stderr, "MISMATCH tx=%d pack=%d lane=%d\n", tx, i, j);
                return 1;
            }
        }
    }
    return 0;
}

}  // namespace

int main() {
    for (int tx = 0; tx < kTransactions; ++tx) {
        if (RunTransaction(tx) != 0) {
            return 1;
        }
    }
    std::printf("vadd cosim: PASS transactions=%d\n", kTransactions);
    return 0;
}
