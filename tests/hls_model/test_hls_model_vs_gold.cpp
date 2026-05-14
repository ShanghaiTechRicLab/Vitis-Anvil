#include <catch_amalgamated.hpp>
#include "accel/gold/saxpy_gold.hpp"
#include "accel/hls/saxpy_hls_model.hpp"
#include "accel/config.hpp"

#include <cstdint>
#include <random>
#include <vector>

using accel::gold::SaxpyConfig;
using accel::gold::saxpy_gold;
using accel::hls::saxpy_hls_model;

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
  run_one(accel::config::kMaxElements, 0.5f, 1234u);
}
