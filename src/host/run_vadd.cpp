#include <anvil/cli/anvil_cli.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <anvil/runtime/xrt_context.hpp>

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
constexpr std::size_t kPackWidth = 16;

struct InputData {
    std::vector<float> a;
    std::vector<float> b;
};

std::size_t RoundUpToPack(std::size_t n) {
    return ((n + kPackWidth - 1) / kPackWidth) * kPackWidth;
}

std::vector<float> ReadFloats(const fs::path& path, std::size_t expected_n) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("open failed: " + path.string());
    const auto bytes = static_cast<std::size_t>(f.tellg());
    if (bytes != expected_n * sizeof(float)) {
        throw std::runtime_error("size mismatch for " + path.string());
    }
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
    return InputData{
        ReadFloats(data_dir / meta.at("x").get<std::string>(), n),
        ReadFloats(data_dir / meta.at("y").get<std::string>(), n),
    };
}
}  // namespace

int main(int argc, char* argv[]) {
    anvil::log::Init("run_vadd");
    anvil::cli::Parser cli("run_vadd");
    cli.add_argument("--xclbin").required().help("Path to xclbin containing vadd_1");
    cli.add_argument("--n").default_value(1024).scan<'i', int>().help("Number of synthetic elements");
    cli.add_argument("--data-dir").help("Dataset directory containing meta.json, x.bin, y.bin");
    cli.add_argument("--output").help("Optional path for writing device output .bin");
    anvil::cli::parse_or_exit(cli, argc, argv);

    const fs::path xclbin_path = cli.get<std::string>("--xclbin");
    int n_arg = cli.get<int>("--n");
    std::vector<float> input_a;
    std::vector<float> input_b;
    try {
        if (auto data_dir_arg = cli.present<std::string>("--data-dir")) {
            InputData input = LoadInputData(*data_dir_arg);
            input_a = std::move(input.a);
            input_b = std::move(input.b);
            n_arg = static_cast<int>(input_a.size());
        }
    } catch (const std::exception& exc) {
        anvil::log::Error("input error: {}", exc.what());
        return 2;
    }
    if (n_arg <= 0) {
        anvil::log::Error("--n must be positive, got {}", n_arg);
        return 2;
    }
    const auto n = static_cast<std::size_t>(n_arg);
    const std::size_t padded_n = RoundUpToPack(n);
    if (input_a.empty()) {
        input_a.resize(n);
        input_b.resize(n);
        std::iota(input_a.begin(), input_a.end(), 0.0F);
        std::fill(input_b.begin(), input_b.end(), 1.0F);
    }

    XrtContext ctx(0, xclbin_path);
    auto kernel = ctx.GetKernel("vadd:{vadd_1}");
    XrtBuffer<float> a_buf(ctx, kernel, 0, padded_n);
    XrtBuffer<float> b_buf(ctx, kernel, 1, padded_n);
    XrtBuffer<float> out_buf(ctx, kernel, 2, padded_n);
    std::fill(a_buf.host(), a_buf.host() + padded_n, 0.0F);
    std::fill(b_buf.host(), b_buf.host() + padded_n, 0.0F);
    std::fill(out_buf.host(), out_buf.host() + padded_n, 0.0F);
    std::copy(input_a.begin(), input_a.end(), a_buf.host());
    std::copy(input_b.begin(), input_b.end(), b_buf.host());
    a_buf.Sync(SyncDirection::HostToDevice);
    b_buf.Sync(SyncDirection::HostToDevice);
    kernel(a_buf.bo(), b_buf.bo(), out_buf.bo(), static_cast<int>(padded_n / kPackWidth));
    out_buf.Sync(SyncDirection::DeviceToHost);

    std::vector<float> gold(n);
    for (std::size_t i = 0; i < n; ++i) gold[i] = input_a[i] + input_b[i];
    const std::span<const float> device_out(out_buf.host(), n);
    if (auto output_arg = cli.present<std::string>("--output")) {
        try { WriteFloats(*output_arg, device_out); }
        catch (const std::exception& exc) { anvil::log::Error("output error: {}", exc.what()); return 2; }
    }
    const double max_err = anvil::compare::MaxAbsError(std::span<const float>(gold), device_out);
    anvil::log::Info("MaxAbsError = {:.2e}", max_err);
    return max_err == 0.0 ? 0 : 1;
}
