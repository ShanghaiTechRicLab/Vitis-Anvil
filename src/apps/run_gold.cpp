// src/apps/run_gold.cpp
//
// Loads a case, runs saxpy_gold, writes the result .bin and a small
// JSON report, and prints a tabulate summary.

#include "anvil/config.hpp"
#include "anvil/gold/saxpy_gold.hpp"

#include <argparse.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tabulate.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
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

std::vector<float> read_floats(const fs::path& path, std::size_t expected_n) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("open failed: " + path.string());
  const auto bytes = static_cast<std::size_t>(f.tellg());
  if (bytes != expected_n * sizeof(float)) {
    throw std::runtime_error(
      "size mismatch for " + path.string() +
      ": expected " + std::to_string(expected_n * sizeof(float)) +
      " bytes, got " + std::to_string(bytes));
  }
  f.seekg(0);
  std::vector<float> v(expected_n);
  f.read(reinterpret_cast<char*>(v.data()),
         static_cast<std::streamsize>(expected_n * sizeof(float)));
  if (!f) throw std::runtime_error("read failed: " + path.string());
  return v;
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

fs::path default_manifest_dir(const fs::path& manifest_path) {
  const fs::path dir = manifest_path.parent_path();
  return dir.empty() ? fs::path{"."} : dir;
}

}  // namespace

int main(int argc, char** argv) {
  argparse::ArgumentParser parser("run_gold");
  parser.add_argument("--case").required().help("path to case manifest JSON");
  parser.add_argument("--data-dir").help("input directory (default: manifest dir)");
  parser.add_argument("--output-dir").help("output directory (default: data-dir)");
  parser.add_argument("--report").help("report filename (default: <case_id>_run_gold.json)");
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

    const fs::path manifest_path = parser.get<std::string>("--case");
    const Manifest m = load_manifest(manifest_path);

    if (m.n == 0) {
      spdlog::error("manifest n must be > 0");
      return kExitInputError;
    }
    if (m.n > static_cast<std::uint64_t>(anvil::config::kMaxElements)) {
      spdlog::error("manifest n={} exceeds kMaxElements={}",
                    m.n, anvil::config::kMaxElements);
      return kExitInputError;
    }

    const fs::path data_dir = parser.present("--data-dir")
        ? fs::path(parser.get<std::string>("--data-dir"))
        : default_manifest_dir(manifest_path);
    const fs::path output_dir = parser.present("--output-dir")
        ? fs::path(parser.get<std::string>("--output-dir"))
        : data_dir;

    if (!fs::exists(data_dir)) {
      spdlog::error("data-dir does not exist: {}", data_dir.string());
      return kExitInputError;
    }
    fs::create_directories(output_dir);

    const auto n = static_cast<std::size_t>(m.n);
    const auto x = read_floats(data_dir / m.x_file, n);
    const auto y = read_floats(data_dir / m.y_file, n);

    std::vector<float> out(n);
    anvil::gold::saxpy_gold(x, y, out, anvil::gold::SaxpyConfig{m.a});

    const std::string out_name = m.case_id + "_gold_out.bin";
    write_floats(output_dir / out_name, out);

    const double mean_v = std::accumulate(out.begin(), out.end(), 0.0)
                        / static_cast<double>(n);
    const float max_v = *std::max_element(out.begin(), out.end());
    const float min_v = *std::min_element(out.begin(), out.end());

    tabulate::Table tab;
    tab.add_row({"case_id", "n", "a", "mean(out)", "max(out)", "min(out)"});
    tab.add_row({m.case_id, std::to_string(n), std::to_string(m.a),
                 std::to_string(mean_v), std::to_string(max_v), std::to_string(min_v)});
    std::cout << tab << "\n";

    const std::string report_name = parser.present("--report")
        ? parser.get<std::string>("--report")
        : m.case_id + "_run_gold.json";
    nlohmann::json report{
      {"case_id", m.case_id},
      {"n", n},
      {"a", m.a},
      {"out_file", out_name},
      {"summary", {{"mean", mean_v}, {"max", max_v}, {"min", min_v}}},
    };
    std::ofstream rf(output_dir / report_name);
    if (!rf) throw std::runtime_error("open report failed: " + (output_dir / report_name).string());
    rf << report.dump(2);
    if (!rf) throw std::runtime_error("write report failed: " + (output_dir / report_name).string());

    spdlog::info("run_gold: wrote {} and {}",
                 (output_dir / out_name).string(),
                 (output_dir / report_name).string());
    return kExitOk;
  }
  catch (const nlohmann::json::exception& e) {
    spdlog::error("manifest JSON error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::runtime_error& e) {
    spdlog::error("run_gold input error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::exception& e) {
    spdlog::error("run_gold: {}", e.what());
    return kExitInternalError;
  }
}
