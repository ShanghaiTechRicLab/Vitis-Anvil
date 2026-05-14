#include <anvil/cli/anvil_cli.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/gold/saxpy_gold.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <anvil/runtime/xrt_context.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <span>
#include <vector>

namespace fs = std::filesystem;
using anvil::runtime::SyncDirection;
using anvil::runtime::XrtBuffer;
using anvil::runtime::XrtContext;

namespace {
constexpr std::size_t kSaxpyPackWidth = 16;

std::size_t RoundUpToPack(std::size_t n) {
    return ((n + kSaxpyPackWidth - 1) / kSaxpyPackWidth) * kSaxpyPackWidth;
}
}  // namespace

int main(int argc, char* argv[]) {
    anvil::log::Init("run_saxpy");

    anvil::cli::Parser cli("run_saxpy");
    cli.add_argument("--xclbin").required().help("Path to saxpy.xclbin");
    cli.add_argument("--n").default_value(1024).scan<'i', int>().help("Number of elements");
    cli.add_argument("--a").default_value(2.0F).scan<'g', float>().help("Scalar coefficient");
    anvil::cli::parse_or_exit(cli, argc, argv);

    const fs::path xclbin_path = cli.get<std::string>("--xclbin");
    const int n_arg = cli.get<int>("--n");
    const float a = cli.get<float>("--a");
    if (n_arg <= 0) {
        anvil::log::Error("--n must be positive, got {}", n_arg);
        return 2;
    }
    const auto n = static_cast<std::size_t>(n_arg);
    const std::size_t padded_n = RoundUpToPack(n);

    anvil::log::Info("opening device 0, xclbin={}", xclbin_path.string());
    XrtContext ctx(0, xclbin_path);
    auto kernel = ctx.GetKernel("saxpy:{saxpy_1}");

    XrtBuffer<float> x_buf(ctx, kernel, 0, padded_n);
    XrtBuffer<float> y_buf(ctx, kernel, 1, padded_n);
    XrtBuffer<float> out_buf(ctx, kernel, 2, padded_n);

    std::fill(x_buf.host(), x_buf.host() + padded_n, 0.0F);
    std::fill(y_buf.host(), y_buf.host() + padded_n, 0.0F);
    std::fill(out_buf.host(), out_buf.host() + padded_n, 0.0F);
    std::iota(x_buf.host(), x_buf.host() + n, 0.0F);
    std::iota(y_buf.host(), y_buf.host() + n, 0.0F);

    x_buf.Sync(SyncDirection::HostToDevice);
    y_buf.Sync(SyncDirection::HostToDevice);

    anvil::log::Info("launching saxpy kernel n={} padded_n={} a={}", n, padded_n, a);
    // Kernel ABI: saxpy(SaxpyPack* x, SaxpyPack* y, SaxpyPack* out, float a, int n_total).
    // SaxpyPack has 16 floats, so host BOs are padded to a full pack.
    // Args 3=a (float), 4=n_total (int) — do not swap these.
    kernel(x_buf.bo(), y_buf.bo(), out_buf.bo(), a, n_arg);

    out_buf.Sync(SyncDirection::DeviceToHost);

    std::vector<float> gold_out(n);
    anvil::gold::saxpy_gold(std::span<const float>(x_buf.host(), n),
                            std::span<const float>(y_buf.host(), n),
                            std::span<float>(gold_out),
                            anvil::gold::SaxpyConfig{a});

    const double max_err = anvil::compare::MaxAbsError(
        std::span<const float>(gold_out), std::span<const float>(out_buf.host(), n));
    anvil::log::Info("MaxAbsError = {:.2e}", max_err);

    if (max_err == 0.0) {
        anvil::log::Info("PASS: bit-exact match");
        return 0;
    }

    anvil::log::Warn("DIFF: max_abs_error = {:.6e}", max_err);
    return 1;
}
