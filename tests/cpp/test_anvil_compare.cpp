#include <catch_amalgamated.hpp>
#include <anvil/compare/element_wise.hpp>
#include <anvil/compare/signal.hpp>
#include <anvil/compare/bitwise.hpp>
#include <anvil/compare/classification.hpp>
#include <anvil/gold/interface.hpp>
#include <anvil/hls/hls_aliases.hpp>
#include <cmath>
#include <vector>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>

using namespace anvil::compare;

TEST_CASE("MaxAbsError: identical vectors -> 0", "[compare][element_wise]") {
    std::vector<float> a{1.0f, 2.0f, 3.0f};
    REQUIRE(MaxAbsError(a, a) == Catch::Approx(0.0));
}

TEST_CASE("MaxAbsError: known diff", "[compare][element_wise]") {
    std::vector<float> a{1.0f, 2.0f, 3.0f};
    std::vector<float> b{1.5f, 2.0f, 2.5f};
    REQUIRE(MaxAbsError(a, b) == Catch::Approx(0.5));
}

TEST_CASE("RmsError: identical -> 0", "[compare][element_wise]") {
    std::vector<float> a{1.0f, 2.0f, 3.0f, 4.0f};
    REQUIRE(RmsError(a, a) == Catch::Approx(0.0));
}

TEST_CASE("RmsError: known diff", "[compare][element_wise]") {
    std::vector<float> a{0.0f, 0.0f};
    std::vector<float> b{1.0f, 1.0f};
    REQUIRE(RmsError(a, b) == Catch::Approx(1.0));
}

TEST_CASE("MeanAbsError: known diff", "[compare][element_wise]") {
    std::vector<float> a{0.0f, 0.0f, 0.0f};
    std::vector<float> b{1.0f, 2.0f, 3.0f};
    REQUIRE(MeanAbsError(a, b) == Catch::Approx(2.0));
}

TEST_CASE("BitExact: identical -> true", "[compare][bitwise]") {
    std::vector<float> a{1.0f, 2.0f};
    REQUIRE(BitExact(a, a));
}

TEST_CASE("BitExact: different -> false", "[compare][bitwise]") {
    std::vector<float> a{1.0f};
    std::vector<float> b{1.5f};
    REQUIRE_FALSE(BitExact(a, b));
}

TEST_CASE("UlpDiff: same value -> 0", "[compare][bitwise]") {
    REQUIRE(UlpDiff(1.0f, 1.0f) == 0u);
}

TEST_CASE("UlpDiff: adjacent floats -> 1", "[compare][bitwise]") {
    float a = 1.0f;
    float b = std::nextafter(a, 2.0f);
    REQUIRE(UlpDiff(a, b) == 1u);
}

TEST_CASE("Psnr: identical -> inf", "[compare][signal]") {
    std::vector<float> a{1.0f, 2.0f, 3.0f};
    double p = Psnr(std::span<const float>(a), std::span<const float>(a), 3.0f);
    REQUIRE(std::isinf(p));
}

TEST_CASE("size mismatch throws", "[compare][element_wise]") {
    std::vector<float> a{1.0f, 2.0f};
    std::vector<float> b{1.0f};
    REQUIRE_THROWS_AS(MaxAbsError(a, b), std::invalid_argument);
}


TEST_CASE("CosineSimilarity: parallel vectors -> 1", "[compare][element_wise]") {
    std::vector<float> a{1.0f, 0.0f};
    REQUIRE(CosineSimilarity(a, a) == Catch::Approx(1.0));
}

TEST_CASE("Snr: identical -> inf", "[compare][signal]") {
    std::vector<float> a{1.0f, 2.0f, 3.0f};
    double s = Snr(std::span<const float>(a), std::span<const float>(a));
    REQUIRE(std::isinf(s));
}

TEST_CASE("classification accuracy helpers", "[compare][classification]") {
    std::vector<int> labels{1, 0};
    std::vector<float> logits{0.1f, 0.9f, 0.8f, 0.2f};
    REQUIRE(Top1Accuracy(labels, logits, 2) == Catch::Approx(1.0));
    REQUIRE(TopKAccuracy(labels, logits, 2, 2) == Catch::Approx(1.0));
}

TEST_CASE("BitExact uses object representation", "[compare][bitwise]") {
    std::vector<float> pos_zero{0.0f};
    std::vector<float> neg_zero{-0.0f};
    REQUIRE_FALSE(BitExact(pos_zero, neg_zero));
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> a{nan};
    std::vector<float> b{nan};
    REQUIRE(BitExact(a, b));
}

TEST_CASE("UlpDiff treats signed zero as equal", "[compare][bitwise]") {
    REQUIRE(UlpDiff(0.0f, -0.0f) == 0u);
}

TEST_CASE("UlpDiff handles opposite sign without overflow", "[compare][bitwise]") {
    REQUIRE(UlpDiff(1.0e30f, -1.0e30f) == 3801343380u);
}

TEST_CASE("gold LoadVector/DumpVector round trip and errors", "[gold][io]") {
    const auto path = std::filesystem::temp_directory_path() / "anvil_gold_vec.bin";
    std::vector<float> data{1.0f, 2.0f, 3.0f};
    REQUIRE_NOTHROW(anvil::gold::DumpVector<float>(path, std::span<const float>(data)));
    auto loaded = anvil::gold::LoadVector<float>(path);
    REQUIRE(BitExact(data, loaded));
    std::filesystem::remove(path);
    REQUIRE_THROWS(anvil::gold::LoadVector<float>(path));
}

TEST_CASE("gold DumpVector reports write failure", "[gold][io]") {
#ifdef __linux__
    std::vector<float> data{1.0f};
    REQUIRE_THROWS(anvil::gold::DumpVector<float>("/dev/full", std::span<const float>(data)));
#else
    SUCCEED("/dev/full unavailable on this platform");
#endif
}

TEST_CASE("GoldFn and HLS aliases compile", "[gold][hls][smoke]") {
    anvil::gold::GoldFn<float, float> fn = [](std::span<const float> in, std::span<float> out) {
        for (std::size_t i = 0; i < in.size(); ++i) out[i] = in[i];
    };
    std::vector<float> in{1.0f}, out(1);
    fn(in, out);
    REQUIRE(out[0] == Catch::Approx(1.0f));
    anvil::hls::DataPack<float, 1> pack;
    pack[0] = 1.0f;
    anvil::hls::Stream<anvil::hls::DataPack<float, 1>> stream("alias_stream");
    stream.Push(pack);
    REQUIRE(stream.Pop()[0] == Catch::Approx(1.0f));
}
