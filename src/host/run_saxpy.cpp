#include <anvil/cli/anvil_cli.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/gold/saxpy_gold.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <anvil/runtime/xrt_context.hpp>

#include "kernels/kernel_types.hpp"

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
constexpr std::size_t kSaxpyPackWidth = static_cast<std::size_t>(kernels::kSaxpyPackWidth);

struct InputData {
    std::vector<float> x;
    std::vector<float> y;
    float a = 2.0F;
};

std::size_t RoundUpToPack(std::size_t n) {
    return ((n + kSaxpyPackWidth - 1) / kSaxpyPackWidth) * kSaxpyPackWidth;
}

std::vector<float> ReadFloats(const fs::path& path, std::size_t expected_n) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        throw std::runtime_error("open failed: " + path.string());
    }
    const auto bytes = static_cast<std::size_t>(f.tellg());
    if (bytes != expected_n * sizeof(float)) {
        throw std::runtime_error("size mismatch for " + path.string() + ": expected " +
                                 std::to_string(expected_n * sizeof(float)) + " bytes, got " +
                                 std::to_string(bytes));
    }
    f.seekg(0);
    std::vector<float> v(expected_n);
    f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
    if (!f) {
        throw std::runtime_error("read failed: " + path.string());
    }
    return v;
}

void WriteFloats(const fs::path& path, std::span<const float> values) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        throw std::runtime_error("open for write failed: " + path.string());
    }
    f.write(reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(values.size() * sizeof(float)));
    if (!f) {
        throw std::runtime_error("write failed: " + path.string());
    }
}

InputData LoadInputData(const fs::path& data_dir) {
    const fs::path meta_path = data_dir / "meta.json";
    std::ifstream f(meta_path);
    if (!f) {
        throw std::runtime_error("meta.json not found: " + meta_path.string());
    }
    nlohmann::json meta;
    f >> meta;
    const auto n = meta.at("n").get<std::size_t>();
    if (n == 0) {
        throw std::runtime_error("manifest n must be > 0");
    }
    InputData input;
    input.a = meta.at("a").get<float>();
    input.x = ReadFloats(data_dir / meta.at("x").get<std::string>(), n);
    input.y = ReadFloats(data_dir / meta.at("y").get<std::string>(), n);
    return input;
}
}  // namespace

int main(int argc, char* argv[]) {
    anvil::log::Init("run_saxpy");

    anvil::cli::Parser cli("run_saxpy");
    cli.add_argument("--xclbin").required().help("Path to saxpy.xclbin");
    cli.add_argument("--n").default_value(1024).scan<'i', int>().help("Number of elements for synthetic input");
    cli.add_argument("--a").default_value(2.0F).scan<'g', float>().help("Scalar coefficient for synthetic input");
    cli.add_argument("--data-dir").help("Dataset directory containing meta.json, x.bin, y.bin");
    cli.add_argument("--output").help("Optional path for writing device output .bin");
    anvil::cli::parse_or_exit(cli, argc, argv);

    const fs::path xclbin_path = cli.get<std::string>("--xclbin");
    int n_arg = cli.get<int>("--n");
    float a = cli.get<float>("--a");
    std::vector<float> input_x;
    std::vector<float> input_y;

    try {
        if (auto data_dir_arg = cli.present<std::string>("--data-dir")) {
            const InputData input = LoadInputData(*data_dir_arg);
            input_x = input.x;
            input_y = input.y;
            a = input.a;
            n_arg = static_cast<int>(input_x.size());
            if (input_x.size() != input_y.size()) {
                anvil::log::Error("dataset x/y size mismatch: {} vs {}", input_x.size(), input_y.size());
                return 2;
            }
            anvil::log::Info("loaded dataset {} n={} a={}", *data_dir_arg, n_arg, a);
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
    if (input_x.empty()) {
        input_x.resize(n);
        input_y.resize(n);
        std::iota(input_x.begin(), input_x.end(), 0.0F);
        std::iota(input_y.begin(), input_y.end(), 0.0F);
    }

    anvil::log::Info("opening device 0, xclbin={}", xclbin_path.string());
    XrtContext ctx(0, xclbin_path);
    auto kernel = ctx.GetKernel("saxpy:{saxpy_1}");

    XrtBuffer<float> x_buf(ctx, kernel, 0, padded_n);
    XrtBuffer<float> y_buf(ctx, kernel, 1, padded_n);
    XrtBuffer<float> out_buf(ctx, kernel, 2, padded_n);

    std::fill(x_buf.host(), x_buf.host() + padded_n, 0.0F);
    std::fill(y_buf.host(), y_buf.host() + padded_n, 0.0F);
    std::fill(out_buf.host(), out_buf.host() + padded_n, 0.0F);
    std::copy(input_x.begin(), input_x.end(), x_buf.host());
    std::copy(input_y.begin(), input_y.end(), y_buf.host());

    x_buf.Sync(SyncDirection::HostToDevice);
    y_buf.Sync(SyncDirection::HostToDevice);

    anvil::log::Info("launching saxpy kernel n={} padded_n={} a={}", n, padded_n, a);
    // Kernel ABI: saxpy(SaxpyPack* x, SaxpyPack* y, SaxpyPack* out, float a, int n_total).
    // SaxpyPack has kernels::kSaxpyPackWidth floats, so host BOs are
    // padded to a full pack.
    // Args 3=a (float), 4=n_total (int) — do not swap these.
    kernel(x_buf.bo(), y_buf.bo(), out_buf.bo(), a, n_arg);

    out_buf.Sync(SyncDirection::DeviceToHost);

    std::vector<float> gold_out(n);
    anvil::gold::saxpy_gold(std::span<const float>(x_buf.host(), n),
                            std::span<const float>(y_buf.host(), n),
                            std::span<float>(gold_out),
                            anvil::gold::SaxpyConfig{a});

    const std::span<const float> device_out(out_buf.host(), n);
    if (auto output_arg = cli.present<std::string>("--output")) {
        try {
            WriteFloats(*output_arg, device_out);
            anvil::log::Info("wrote device output {}", *output_arg);
        } catch (const std::exception& exc) {
            anvil::log::Error("output error: {}", exc.what());
            return 2;
        }
    }

    const double max_err = anvil::compare::MaxAbsError(
        std::span<const float>(gold_out), device_out);
    anvil::log::Info("MaxAbsError = {:.2e}", max_err);

    if (max_err == 0.0) {
        anvil::log::Info("PASS: bit-exact match");
        return 0;
    }

    anvil::log::Warn("DIFF: max_abs_error = {:.6e}", max_err);
    return 1;
}
