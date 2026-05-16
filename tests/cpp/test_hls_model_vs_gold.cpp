#include <catch_amalgamated.hpp>
#include "gold/saxpy_gold.hpp"
#include "gold/vadd_gold.hpp"
#include "hls_model/saxpy_hls_model.hpp"
#include "hls_model/vadd_hls_model.hpp"
#include "anvil/config.hpp"
#include "kernels/kernel_types.hpp"

#include <cstdint>
#include <random>
#include <vector>

using gold::SaxpyConfig;
using gold::saxpy_gold;
using hls_model::saxpy_hls_model;

namespace {

void run_one(std::size_t n, float a, std::uint32_t seed) {
  std::vector<float> x(n), y(n), out_g(n), out_h(n);
  std::mt19937 rng{seed};
  std::uniform_real_distribution<float> dist{-10.0f, 10.0f};
  for (auto& v : x) v = dist(rng);
  for (auto& v : y) v = dist(rng);

  SaxpyConfig cfg{a};
  saxpy_gold     (x, y, out_g, cfg);
  saxpy_hls_model(x, y, out_h, cfg);

  for (std::size_t i = 0; i < n; ++i) {
    INFO("n=" << n << " a=" << a << " i=" << i
         << " gold=" << out_g[i] << " hls=" << out_h[i]);
    REQUIRE(out_g[i] == out_h[i]);   // bit-exact
  }
}

void run_vadd_one(std::size_t n, std::uint32_t seed) {
  std::vector<float> a(n), b(n), out_g(n), out_h(n);
  std::mt19937 rng{seed};
  std::uniform_real_distribution<float> dist{-100.0f, 100.0f};
  for (auto& v : a) v = dist(rng);
  for (auto& v : b) v = dist(rng);

  gold::vadd_gold(a, b, out_g);
  hls_model::vadd_hls_model(a, b, out_h);

  for (std::size_t i = 0; i < n; ++i) {
    INFO("vadd n=" << n << " i=" << i << " gold=" << out_g[i] << " hls=" << out_h[i]);
    REQUIRE(out_g[i] == out_h[i]);
  }
}

}  // namespace

TEST_CASE("hls_model vs gold: bit-exact across sizes", "[hls][parity]") {
  for (std::size_t n : {std::size_t{1}, std::size_t{7}, std::size_t{8},
                        std::size_t{9}, std::size_t{64}, std::size_t{4096}}) {
    DYNAMIC_SECTION("n=" << n) {
      run_one(n, 1.0f, 42u);
    }
  }
}

TEST_CASE("hls_model vs gold: bit-exact across a", "[hls][parity]") {
  for (float a : {0.0f, 1.0f, -1.0f, 2.5f, -3.14159f}) {
    DYNAMIC_SECTION("a=" << a) {
      run_one(128, a, 7u);
    }
  }
}

TEST_CASE("hls_model vs gold: bit-exact at kMaxElements", "[hls][parity][slow]") {
  run_one(anvil::config::kMaxElements, 0.5f, 1234u);
}

TEST_CASE("vadd hls_model vs gold: bit-exact across pack boundaries", "[hls][parity][vadd]") {
  for (std::size_t n : {std::size_t{0},
                        std::size_t{1},
                        static_cast<std::size_t>(kernels::kVaddPackWidth - 1),
                        static_cast<std::size_t>(kernels::kVaddPackWidth),
                        static_cast<std::size_t>(kernels::kVaddPackWidth + 1)}) {
    DYNAMIC_SECTION("n=" << n) {
      run_vadd_one(n, 99u);
    }
  }
}

TEST_CASE("vadd hls_model vs gold: randomized larger vector", "[hls][parity][vadd]") {
  run_vadd_one(static_cast<std::size_t>(kernels::kVaddPackWidth * 17 + 5), 123u);
}
