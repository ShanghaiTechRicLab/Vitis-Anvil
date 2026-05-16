#include <catch_amalgamated.hpp>

#include "anvil/config.hpp"
#include "anvil/hls/dataflow.hpp"
#include "anvil/hls/pack.hpp"
#include "anvil/hls/packed_ops.hpp"
#include "anvil/hls/stream.hpp"
#include "kernels/kernel_types.hpp"

TEST_CASE("anvil hls pack exposes width and lane helpers", "[hls]") {
  anvil::hls::Pack<float, 4> pack;
  for (int i = 0; i < 4; ++i) {
    anvil::hls::SetLane(pack, i, static_cast<float>(i + 1));
  }

  STATIC_REQUIRE(anvil::hls::PackTraits<anvil::hls::Pack<float, 4>>::width == 4);
  REQUIRE(anvil::hls::GetLane(pack, 0) == 1.0f);
  REQUIRE(anvil::hls::GetLane(pack, 3) == 4.0f);
}

TEST_CASE("anvil hls stream alias compiles", "[hls]") {
  anvil::hls::Stream<int, 2> stream("unit_stream");
  stream.Push(7);
  REQUIRE(stream.Pop() == 7);
}

TEST_CASE("kernel pack aliases share the canonical hls pack wrapper", "[hls]") {
  STATIC_REQUIRE(kernels::kSaxpyPackWidth == anvil::config::kParallelism);
  STATIC_REQUIRE(kernels::kVaddPackWidth == 16);
  STATIC_REQUIRE(kernels::kPipelinePackWidth == 16);

  kernels::SaxpyPack saxpy_pack;
  anvil::hls::SetLane(saxpy_pack, 0, 3.0f);
  REQUIRE(anvil::hls::GetLane(saxpy_pack, 0) == 3.0f);
}

namespace {
struct UnitSaxpyOp {
  float operator()(float scalar, float x, float y) const { return scalar * x + y; }
};

struct UnitVaddOp {
  float operator()(float x, float y) const { return x + y; }
};
}  // namespace

TEST_CASE("packed ops load map and store streams", "[hls]") {
  typedef anvil::hls::Pack<float, 4> Pack4;
  Pack4 x[2];
  Pack4 y[2];
  Pack4 out[2];
  for (int i = 0; i < 2; ++i) {
    for (int lane = 0; lane < 4; ++lane) {
      anvil::hls::SetLane(x[i], lane, static_cast<float>(i * 4 + lane));
      anvil::hls::SetLane(y[i], lane, 10.0f);
    }
  }

  anvil::hls::Stream<Pack4, 4> sx("sx"), sy("sy"), so("so");
  anvil::hls::LoadPacks(x, sx, 2);
  anvil::hls::LoadPacks(y, sy, 2);
  anvil::hls::MapPacksWithScalar<Pack4>(sx, sy, so, 2.0f, 2, UnitSaxpyOp());
  anvil::hls::StorePacks(so, out, 2);

  REQUIRE(anvil::hls::GetLane(out[0], 0) == 10.0f);
  REQUIRE(anvil::hls::GetLane(out[1], 3) == 24.0f);
}

TEST_CASE("packed ops map two streams without scalar", "[hls]") {
  typedef anvil::hls::Pack<float, 4> Pack4;
  Pack4 a[1];
  Pack4 b[1];
  for (int lane = 0; lane < 4; ++lane) {
    anvil::hls::SetLane(a[0], lane, static_cast<float>(lane));
    anvil::hls::SetLane(b[0], lane, 10.0f);
  }

  anvil::hls::Stream<Pack4, 2> sa("sa"), sb("sb"), so("so");
  anvil::hls::LoadPacks(a, sa, 1);
  anvil::hls::LoadPacks(b, sb, 1);
  anvil::hls::MapPacks<Pack4>(sa, sb, so, 1, UnitVaddOp());
  Pack4 out[1];
  anvil::hls::StorePacks(so, out, 1);

  REQUIRE(anvil::hls::GetLane(out[0], 0) == 10.0f);
  REQUIRE(anvil::hls::GetLane(out[0], 3) == 13.0f);
}

TEST_CASE("packed ops accept zero packs", "[hls]") {
  typedef anvil::hls::Pack<float, 4> Pack4;
  anvil::hls::Stream<Pack4, 2> sa("zero_a"), sb("zero_b"), so("zero_o");
  anvil::hls::MapPacks<Pack4>(sa, sb, so, 0, UnitVaddOp());
  SUCCEED("zero-pack map is a no-op");
}

TEST_CASE("packed ops map memory to memory", "[hls]") {
  typedef anvil::hls::Pack<float, 4> Pack4;
  Pack4 a[1];
  Pack4 b[1];
  Pack4 out[1];
  for (int lane = 0; lane < 4; ++lane) {
    anvil::hls::SetLane(a[0], lane, static_cast<float>(lane));
    anvil::hls::SetLane(b[0], lane, 1.0f);
  }
  anvil::hls::MapMem2Packs(a, b, out, 1, UnitVaddOp());
  REQUIRE(anvil::hls::GetLane(out[0], 0) == 1.0f);
  REQUIRE(anvil::hls::GetLane(out[0], 3) == 4.0f);
}
