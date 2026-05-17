#include <anvil/cli/anvil_cli.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <anvil/runtime/xrt_context.hpp>

#include "kernels/abi.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using anvil::runtime::SyncDirection;
using anvil::runtime::XrtBuffer;
using anvil::runtime::XrtContext;

namespace {
constexpr std::size_t kPackWidth = static_cast<std::size_t>(kernels::kPipelinePackWidth);

struct InputData { std::vector<float> x; std::vector<float> y; float a = 2.0F; };
std::size_t RoundUpToPack(std::size_t n) { return ((n + kPackWidth - 1) / kPackWidth) * kPackWidth; }

std::vector<float> ReadFloats(const fs::path& path, std::size_t expected_n) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("open failed: " + path.string());
    const auto bytes = static_cast<std::size_t>(f.tellg());
    if (bytes != expected_n * sizeof(float)) throw std::runtime_error("size mismatch for " + path.string());
    f.seekg(0);
    std::vector<float> v(expected_n);
    f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
    if (!f) throw std::runtime_error("read failed: " + path.string());
    return v;
}

void WriteFloats(const fs::path& path, std::span<const float> values) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("open for write failed: " + path.string());
    f.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
    if (!f) throw std::runtime_error("write failed: " + path.string());
}

InputData LoadInputData(const fs::path& data_dir) {
    std::ifstream f(data_dir / "meta.json");
    if (!f) throw std::runtime_error("meta.json not found: " + (data_dir / "meta.json").string());
    nlohmann::json meta;
    f >> meta;
    const auto n = meta.at("n").get<std::size_t>();
    return InputData{ReadFloats(data_dir / meta.at("x").get<std::string>(), n),
                     ReadFloats(data_dir / meta.at("y").get<std::string>(), n),
                     meta.at("a").get<float>()};
}
}  // namespace

int main(int argc, char* argv[]) {
    anvil::log::Init("run_pipeline_demo");
    anvil::cli::Parser cli("run_pipeline_demo");
    cli.add_argument("--xclbin").required().help("Path to pipeline_demo.xclbin");
    cli.add_argument("--n").default_value(1024).scan<'i', int>().help("Number of synthetic elements");
    cli.add_argument("--a").default_value(2.0F).scan<'g', float>().help("SAXPY coefficient");
    cli.add_argument("--b").default_value(1.0F).scan<'g', float>().help("Synthetic vadd b value");
    cli.add_argument("--data-dir").help("Dataset directory containing meta.json, x.bin, y.bin");
    cli.add_argument("--output").help("Optional path for writing device output .bin");
    anvil::cli::parse_or_exit(cli, argc, argv);

    const fs::path xclbin_path = cli.get<std::string>("--xclbin");
    int n_arg = cli.get<int>("--n");
    float coeff = cli.get<float>("--a");
    const float b_value = cli.get<float>("--b");
    std::vector<float> x;
    std::vector<float> y;
    try {
        if (auto data_dir_arg = cli.present<std::string>("--data-dir")) {
            InputData input = LoadInputData(*data_dir_arg);
            x = std::move(input.x);
            y = std::move(input.y);
            coeff = input.a;
            n_arg = static_cast<int>(x.size());
        }
    } catch (const std::exception& exc) {
        anvil::log::Error("input error: {}", exc.what());
        return 2;
    }
    if (n_arg <= 0) { anvil::log::Error("--n must be positive, got {}", n_arg); return 2; }
    const auto n = static_cast<std::size_t>(n_arg);
    const std::size_t padded_n = RoundUpToPack(n);
    const int n_packs = static_cast<int>(padded_n / kPackWidth);
    if (x.empty()) {
        x.resize(n); y.resize(n);
        std::iota(x.begin(), x.end(), 0.0F);
        std::iota(y.begin(), y.end(), 0.0F);
    }

    XrtContext ctx(0, xclbin_path);
    auto saxpy = ctx.GetKernel("saxpy_stream:{saxpy_stream_1}");
    auto vadd = ctx.GetKernel("vadd_stream:{vadd_stream_1}");
    XrtBuffer<float> x_buf(ctx, saxpy, 0, padded_n);
    XrtBuffer<float> y_buf(ctx, saxpy, 1, padded_n);
    // Host-visible memory args for vadd_stream(hls::stream&, b, out, n_packs) are
    // b=arg0 and out=arg1. The AXI stream argument has no host BO group.
    XrtBuffer<float> b_buf(ctx, vadd, 0, padded_n);
    XrtBuffer<float> out_buf(ctx, vadd, 1, padded_n);
    std::fill(x_buf.host(), x_buf.host() + padded_n, 0.0F);
    std::fill(y_buf.host(), y_buf.host() + padded_n, 0.0F);
    std::fill(b_buf.host(), b_buf.host() + padded_n, b_value);
    std::fill(out_buf.host(), out_buf.host() + padded_n, 0.0F);
    std::copy(x.begin(), x.end(), x_buf.host());
    std::copy(y.begin(), y.end(), y_buf.host());
    x_buf.Sync(SyncDirection::HostToDevice);
    y_buf.Sync(SyncDirection::HostToDevice);
    b_buf.Sync(SyncDirection::HostToDevice);

    auto r1 = saxpy.Launch(x_buf.bo(), y_buf.bo(), coeff, n_packs);
    auto r2 = vadd.Launch(b_buf.bo(), out_buf.bo(), n_packs);
    r1.wait();
    r2.wait();
    out_buf.Sync(SyncDirection::DeviceToHost);

    std::vector<float> gold(n);
    for (std::size_t i = 0; i < n; ++i) gold[i] = coeff * x[i] + y[i] + b_value;
    const std::span<const float> device_out(out_buf.host(), n);
    if (auto output_arg = cli.present<std::string>("--output")) {
        try { WriteFloats(*output_arg, device_out); }
        catch (const std::exception& exc) { anvil::log::Error("output error: {}", exc.what()); return 2; }
    }
    const double max_err = anvil::compare::MaxAbsError(std::span<const float>(gold), device_out);
    anvil::log::Info("MaxAbsError = {:.2e}", max_err);
    return max_err == 0.0 ? 0 : 1;
}
