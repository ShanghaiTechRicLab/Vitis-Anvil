// src/apps/compare_gold_hls_model.cpp
//
// Runs both saxpy_gold and saxpy_hls_model on the same manifested input,
// compares them via max_abs_error / rms_error, writes a JSON report and
// exits with code 1 if tolerance is exceeded.

#include "anvil/config.hpp"
#include "anvil/gold/metrics.hpp"
#include "anvil/gold/saxpy_gold.hpp"
#include "hls_model/saxpy_hls_model.hpp"

#include <argparse.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tabulate.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kExitOk            = 0;
constexpr int kExitVerdictFail   = 1;
constexpr int kExitInputError    = 2;
constexpr int kExitInternalError = 3;

struct Manifest {
  std::string case_id;
  std::uint64_t n    = 0;
  float a            = 0.0f;
  std::uint64_t seed = 0;
  std::string x_file;
  std::string y_file;
  float tol_max_abs = 0.0f;
  float tol_rms     = 0.0f;
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
  if (j.contains("tolerance")) {
    const auto& t = j["tolerance"];
    m.tol_max_abs = t.value("max_abs", 0.0f);
    m.tol_rms     = t.value("rms",     0.0f);
  }
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
  argparse::ArgumentParser parser("compare_gold_hls_model");
  parser.add_argument("--case").required();
  parser.add_argument("--data-dir");
  parser.add_argument("--output-dir");
  parser.add_argument("--report");
  parser.add_argument("--max-abs").scan<'g', float>().help("override manifest tolerance.max_abs");
  parser.add_argument("--rms").scan<'g', float>().help("override manifest tolerance.rms");
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
    Manifest m = load_manifest(manifest_path);

    if (m.n == 0) {
      spdlog::error("manifest n must be > 0");
      return kExitInputError;
    }
    if (m.n > static_cast<std::uint64_t>(anvil::config::kMaxElements)) {
      spdlog::error("manifest n={} exceeds kMaxElements={}",
                    m.n, anvil::config::kMaxElements);
      return kExitInputError;
    }

    if (auto v = parser.present<float>("--max-abs")) m.tol_max_abs = *v;
    if (auto v = parser.present<float>("--rms"))     m.tol_rms     = *v;

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

    std::vector<float> out_gold(n), out_hls(n);
    anvil::gold::SaxpyConfig cfg{m.a};
    anvil::gold::saxpy_gold     (x, y, out_gold, cfg);
    hls_model::saxpy_hls_model(x, y, out_hls,  cfg);

    const float max_abs = anvil::gold::max_abs_error(out_gold, out_hls);
    const float rms     = anvil::gold::rms_error    (out_gold, out_hls);
    const bool pass     = (max_abs <= m.tol_max_abs) && (rms <= m.tol_rms);
    const std::string verdict = pass ? "pass" : "fail";

    tabulate::Table tab;
    tab.add_row({"case_id", "n", "max_abs", "rms",
                 "tol_max_abs", "tol_rms", "verdict"});
    tab.add_row({m.case_id, std::to_string(n),
                 std::to_string(max_abs), std::to_string(rms),
                 std::to_string(m.tol_max_abs), std::to_string(m.tol_rms),
                 verdict});
    std::cout << tab << "\n";

    const std::string report_name = parser.present("--report")
        ? parser.get<std::string>("--report")
        : m.case_id + "_compare.json";
    nlohmann::json report{
      {"case_id", m.case_id},
      {"n", n},
      {"a", m.a},
      {"max_abs_error", max_abs},
      {"rms_error", rms},
      {"tolerance", {{"max_abs", m.tol_max_abs}, {"rms", m.tol_rms}}},
      {"verdict", verdict},
    };
    std::ofstream rf(output_dir / report_name);
    if (!rf) throw std::runtime_error("open report failed: " + (output_dir / report_name).string());
    rf << report.dump(2);
    if (!rf) throw std::runtime_error("write report failed: " + (output_dir / report_name).string());

    spdlog::info("compare: max_abs={} rms={} -> {}", max_abs, rms, verdict);
    return pass ? kExitOk : kExitVerdictFail;
  }
  catch (const nlohmann::json::exception& e) {
    spdlog::error("manifest JSON error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::runtime_error& e) {
    spdlog::error("compare input error: {}", e.what());
    return kExitInputError;
  }
  catch (const std::exception& e) {
    spdlog::error("compare: {}", e.what());
    return kExitInternalError;
  }
}
