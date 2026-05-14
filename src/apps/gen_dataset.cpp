// src/apps/gen_dataset.cpp
//
// Reads a case manifest JSON and writes deterministic x.bin and y.bin
// for use by run_gold and compare_gold_hls_model.

#include "accel/config.hpp"

#include <argparse.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kExitOk            = 0;
constexpr int kExitInputError    = 2;
constexpr int kExitInternalError = 3;

struct Manifest {
  std::string case_id;
  std::uint64_t n    = 0;
  float a            = 0.0f;
  std::uint64_t seed = 0;
  std::string x_file;
  std::string y_file;
};

Manifest load_manifest(const fs::path& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("manifest open failed: " + path.string());
  nlohmann::json j;
  f >> j;
  Manifest m;
  m.case_id = j.at("case_id").get<std::string>();
  m.n       = j.at("n").get<std::uint64_t>();
  m.a       = j.at("a").get<float>();
  m.seed    = j.at("seed").get<std::uint64_t>();
  m.x_file  = j.at("x").get<std::string>();
  m.y_file  = j.at("y").get<std::string>();
  return m;
}

void write_floats(const fs::path& path, const std::vector<float>& v) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) throw std::runtime_error("open for write failed: " + path.string());
  f.write(reinterpret_cast<const char*>(v.data()),
          static_cast<std::streamsize>(v.size() * sizeof(float)));
  if (!f) throw std::runtime_error("write failed: " + path.string());
}

void set_log_level(const std::string& lvl) {
  if      (lvl == "trace") spdlog::set_level(spdlog::level::trace);
  else if (lvl == "debug") spdlog::set_level(spdlog::level::debug);
  else if (lvl == "info" ) spdlog::set_level(spdlog::level::info);
  else if (lvl == "warn" ) spdlog::set_level(spdlog::level::warn);
  else if (lvl == "err"  ) spdlog::set_level(spdlog::level::err);
  else throw std::runtime_error("unknown --log-level: " + lvl);
}

}  // namespace

int main(int argc, char** argv) {
  argparse::ArgumentParser parser("gen_dataset");
  parser.add_argument("--manifest").required().help("path to case manifest JSON");
  parser.add_argument("--data-dir").help("directory for generated .bin files (default: manifest dir)");
  parser.add_argument("--log-level").default_value(std::string{"info"});

  try {
    parser.parse_args(argc, argv);
  }
  catch (const std::exception& e) {
    spdlog::error("argparse: {}", e.what());
    return kExitInputError;
  }

  try {
    set_log_level(parser.get<std::string>("--log-level"));

    const fs::path manifest_path = parser.get<std::string>("--manifest");
    const Manifest m = load_manifest(manifest_path);

    if (m.n == 0) {
      spdlog::error("manifest n must be > 0");
      return kExitInputError;
    }
    if (m.n > static_cast<std::uint64_t>(accel::config::kMaxElements)) {
      spdlog::error("manifest n={} exceeds kMaxElements={}",
                    m.n, accel::config::kMaxElements);
      return kExitInputError;
    }

    fs::path data_dir = parser.present("--data-dir")
        ? fs::path(parser.get<std::string>("--data-dir"))
        : manifest_path.parent_path();
    if (data_dir.empty()) data_dir = fs::path{"."};
    fs::create_directories(data_dir);

    std::mt19937 rng{static_cast<std::uint32_t>(m.seed)};
    std::uniform_real_distribution<float> dist{-10.0f, 10.0f};

    const auto n = static_cast<std::size_t>(m.n);
    std::vector<float> x(n), y(n);
    for (auto& v : x) v = dist(rng);
    for (auto& v : y) v = dist(rng);

    write_floats(data_dir / m.x_file, x);
    write_floats(data_dir / m.y_file, y);

    spdlog::info("gen_dataset: wrote {}/{} ({}B) and {}/{} ({}B)",
                 data_dir.string(), m.x_file, n * sizeof(float),
                 data_dir.string(), m.y_file, n * sizeof(float));
    return kExitOk;
  }
  catch (const nlohmann::json::exception& e) {
    spdlog::error("manifest JSON error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::runtime_error& e) {
    spdlog::error("gen_dataset input error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::exception& e) {
    spdlog::error("gen_dataset: {}", e.what());
    return kExitInternalError;
  }
}
