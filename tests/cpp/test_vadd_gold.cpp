#include <catch_amalgamated.hpp>

#include "gold/vadd_gold.hpp"

#include <array>
#include <stdexcept>
#include <vector>

TEST_CASE("vadd_gold: empty input is a no-op", "[gold][vadd]") {
  std::vector<float> a, b, out;
  REQUIRE_NOTHROW(gold::vadd_gold(a, b, out));
  REQUIRE(out.empty());
}

TEST_CASE("vadd_gold: adds elementwise", "[gold][vadd]") {
  std::array<float, 4> a{1.0f, -2.0f, 3.5f, 0.0f};
  std::array<float, 4> b{10.0f, 4.0f, -1.5f, 7.0f};
  std::array<float, 4> out{};

  gold::vadd_gold(a, b, out);

  REQUIRE(out[0] == 11.0f);
  REQUIRE(out[1] == 2.0f);
  REQUIRE(out[2] == 2.0f);
  REQUIRE(out[3] == 7.0f);
}

TEST_CASE("vadd_gold: size mismatch throws", "[gold][vadd]") {
  std::vector<float> a(4, 1.0f);
  std::vector<float> b(3, 2.0f);
  std::vector<float> out(4);

  REQUIRE_THROWS_AS(gold::vadd_gold(a, b, out), std::invalid_argument);
  REQUIRE_THROWS_AS(gold::vadd_gold(std::span<const float>(a), std::span<const float>(a), std::span<float>(b)),
                    std::invalid_argument);
}
