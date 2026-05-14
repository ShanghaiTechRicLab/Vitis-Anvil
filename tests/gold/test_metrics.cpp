#include <catch_amalgamated.hpp>
#include "accel/gold/metrics.hpp"

#include <array>
#include <stdexcept>
#include <vector>

using accel::gold::max_abs_error;
using accel::gold::rms_error;

TEST_CASE("max_abs_error: empty inputs return zero", "[gold][metrics]") {
  std::vector<float> a, b;
  REQUIRE(max_abs_error(a, b) == 0.0f);
}

TEST_CASE("max_abs_error: equal arrays return zero", "[gold][metrics]") {
  std::array<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
  std::array<float, 4> b{1.0f, 2.0f, 3.0f, 4.0f};
  REQUIRE(max_abs_error(a, b) == 0.0f);
}

TEST_CASE("max_abs_error: picks the largest absolute difference", "[gold][metrics]") {
  std::array<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
  std::array<float, 4> b{1.0f, 2.5f, 3.0f, 2.0f};
  REQUIRE(max_abs_error(a, b) == 2.0f);   // |4 - 2| dominates
}

TEST_CASE("max_abs_error: size mismatch throws", "[gold][metrics]") {
  std::array<float, 3> a{};
  std::array<float, 4> b{};
  REQUIRE_THROWS_AS(max_abs_error(a, b), std::invalid_argument);
}

TEST_CASE("rms_error: empty inputs return zero", "[gold][metrics]") {
  std::vector<float> a, b;
  REQUIRE(rms_error(a, b) == 0.0f);
}

TEST_CASE("rms_error: equal arrays return zero", "[gold][metrics]") {
  std::array<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
  std::array<float, 4> b{1.0f, 2.0f, 3.0f, 4.0f};
  REQUIRE(rms_error(a, b) == 0.0f);
}

TEST_CASE("rms_error: simple manual computation", "[gold][metrics]") {
  // diffs = {0, 1, 2, 3} -> mean(squared) = (0+1+4+9)/4 = 3.5 -> sqrt(3.5)
  std::array<float, 4> a{0.0f, 1.0f, 2.0f, 3.0f};
  std::array<float, 4> b{0.0f, 0.0f, 0.0f, 0.0f};
  const float expected = 1.870828693f;   // sqrt(3.5)
  REQUIRE(rms_error(a, b) == Catch::Approx(expected).margin(1e-5f));
}

TEST_CASE("rms_error: size mismatch throws", "[gold][metrics]") {
  std::array<float, 3> a{};
  std::array<float, 4> b{};
  REQUIRE_THROWS_AS(rms_error(a, b), std::invalid_argument);
}
