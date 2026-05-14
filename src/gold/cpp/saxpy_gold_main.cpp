// CLI entry point for the C++ gold reference binary.
#include <anvil/cli/anvil_cli.hpp>
#include <anvil/gold/interface.hpp>
#include <anvil/gold/saxpy_gold.hpp>
#include <anvil/json/anvil_json.hpp>
#include <anvil/log/anvil_log.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    anvil::log::Init("saxpy_gold");

    anvil::cli::Parser cli("saxpy_gold_bin");
    cli.add_argument("--data-dir").required().help("Dir containing x.bin, y.bin, meta.json");
    cli.add_argument("--output-dir").default_value(std::string{""}).help("Output dir (default: data-dir)");
    anvil::cli::parse_or_exit(cli, argc, argv);

    const fs::path data_dir = cli.get<std::string>("--data-dir");
    const std::string output_arg = cli.get<std::string>("--output-dir");
    const fs::path output_dir = output_arg.empty() ? data_dir : fs::path{output_arg};
    fs::create_directories(output_dir);

    const auto meta = anvil::json::LoadFile(data_dir / "meta.json");
    const int n = meta.at("n").get<int>();
    const float a = meta.at("a").get<float>();
    if (n <= 0) {
        anvil::log::Error("manifest n must be > 0, got {}", n);
        return 2;
    }

    anvil::log::Info("loading x.bin/y.bin (n={} a={})", n, a);
    auto x = anvil::gold::LoadVector<float>(data_dir / meta.value("x", "x.bin"));
    auto y = anvil::gold::LoadVector<float>(data_dir / meta.value("y", "y.bin"));
    if (x.size() != static_cast<std::size_t>(n) || y.size() != static_cast<std::size_t>(n)) {
        anvil::log::Error("input size mismatch: n={} x={} y={}", n, x.size(), y.size());
        return 2;
    }

    std::vector<float> out(static_cast<std::size_t>(n));
    anvil::gold::saxpy_gold(std::span<const float>(x),
                            std::span<const float>(y),
                            std::span<float>(out),
                            anvil::gold::SaxpyConfig{a});

    const fs::path out_path = output_dir / "gold_out.bin";
    anvil::gold::DumpVector<float>(out_path, std::span<const float>(out));
    anvil::log::Info("wrote {}", out_path.string());
    return 0;
}
