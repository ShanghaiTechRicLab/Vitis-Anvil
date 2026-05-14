#include <catch_amalgamated.hpp>
#include "accel/gold/saxpy_gold.hpp"
#include "accel/config.hpp"

#include <array>
#include <random>
#include <stdexcept>
#include <vector>

using accel::gold::saxpy_gold;
using accel::gold::SaxpyConfig;

TEST_CASE("saxpy_gold: empty input is a no-op", "[gold][saxpy]") {
  std::vector<float> x, y, out;
  SaxpyConfig cfg{2.0f};
  REQUIRE_NOTHROW(saxpy_gold(x, y, out, cfg));
  REQUIRE(out.empty());
}

TEST_CASE("saxpy_gold: n == 1", "[gold][saxpy]") {
  std::array<float, 1> x{3.0f};
  std::array<float, 1> y{4.0f};
  std::array<float, 1> out{};
  saxpy_gold(x, y, out, SaxpyConfig{2.0f});
  REQUIRE(out[0] == 10.0f);   // 2*3 + 4
}

TEST_CASE("saxpy_gold: hardcoded golden values", "[gold][saxpy]") {
  std::array<float, 4> x{1.0f, 2.0f, 3.0f, 4.0f};
  std::array<float, 4> y{10.0f, 20.0f, 30.0f, 40.0f};
  std::array<float, 4> out{};
  saxpy_gold(x, y, out, SaxpyConfig{2.0f});
  REQUIRE(out[0] == 12.0f);
  REQUIRE(out[1] == 24.0f);
  REQUIRE(out[2] == 36.0f);
  REQUIRE(out[3] == 48.0f);
}

TEST_CASE("saxpy_gold: n == 7 (non-aligned)", "[gold][saxpy]") {
  std::vector<float> x(7), y(7), out(7);
  for (int i = 0; i < 7; ++i) {
    x[i] = static_cast<float>(i);
    y[i] = static_cast<float>(i * 10);
  }
  saxpy_gold(x, y, out, SaxpyConfig{1.5f});
  for (int i = 0; i < 7; ++i) {
    REQUIRE(out[i] == 1.5f * static_cast<float>(i) + static_cast<float>(i * 10));
  }
}

TEST_CASE("saxpy_gold: n == kMaxElements", "[gold][saxpy]") {
  const std::size_t n = accel::config::kMaxElements;
  std::vector<float> x(n, 1.0f), y(n, 2.0f), out(n);
  saxpy_gold(x, y, out, SaxpyConfig{3.0f});
  REQUIRE(out.front() == 5.0f);
  REQUIRE(out.back() == 5.0f);
}

TEST_CASE("saxpy_gold: size mismatch throws", "[gold][saxpy]") {
  std::vector<float> x(4, 1.0f);
  std::vector<float> y(3, 2.0f);   // wrong size
  std::vector<float> out(4);
  REQUIRE_THROWS_AS(saxpy_gold(x, y, out, SaxpyConfig{}), std::invalid_argument);
}

TEST_CASE("saxpy_gold: deterministic across repeated calls", "[gold][saxpy]") {
  std::mt19937 rng{42};
  std::uniform_real_distribution<float> dist{-10.0f, 10.0f};
  std::vector<float> x(64), y(64), out_a(64), out_b(64);
  for (auto& v : x) v = dist(rng);
  for (auto& v : y) v = dist(rng);

  SaxpyConfig cfg{0.5f};
  saxpy_gold(x, y, out_a, cfg);
  saxpy_gold(x, y, out_b, cfg);
  for (std::size_t i = 0; i < 64; ++i) {
    REQUIRE(out_a[i] == out_b[i]);
  }
}
