#include <anvil/compare/element_wise.hpp>
#include <anvil/hls/pack.hpp>
#include <anvil/hls/stream.hpp>

#include <cstdio>
#include <span>
#include <vector>

int main() {
    std::vector<float> a{1.0F, 2.0F, 3.0F};
    std::vector<float> b{1.0F, 2.0F, 3.0F};
    const double err = anvil::compare::MaxAbsError(std::span<const float>(a), std::span<const float>(b));

    anvil::hls::Pack<float, 4> pack;
    anvil::hls::SetLane(pack, 0, 7.0F);
    anvil::hls::Stream<anvil::hls::Pack<float, 4>, 2> stream("install_smoke_stream");
    stream.Push(pack);
    const auto out = stream.Pop();
    const bool hls_ok = anvil::hls::GetLane(out, 0) == 7.0F;

    std::printf("install_smoke: MaxAbsError = %g hls_ok=%d\n", err, hls_ok ? 1 : 0);
    return (err == 0.0 && hls_ok) ? 0 : 1;
}
