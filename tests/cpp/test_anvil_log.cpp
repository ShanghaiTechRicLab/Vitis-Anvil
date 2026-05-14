#include <catch_amalgamated.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/cli/anvil_cli.hpp>
#include <anvil/json/anvil_json.hpp>
#include <anvil/toml/anvil_toml.hpp>
#include <anvil/table/anvil_table.hpp>
#include <anvil/progress/anvil_progress.hpp>
#include <anvil/test/anvil_test.hpp>

TEST_CASE("anvil::log Init and Info do not throw", "[log][smoke]") {
    REQUIRE_NOTHROW(anvil::log::Init("test"));
    REQUIRE_NOTHROW(anvil::log::Info("hello {}", "world"));
    REQUIRE_NOTHROW(anvil::log::SetLevel(anvil::log::Level::Warn));
}

TEST_CASE("anvil::json LoadFile throws on missing file", "[json]") {
    REQUIRE_THROWS(anvil::json::LoadFile("/nonexistent/path.json"));
}

TEST_CASE("anvil::toml LoadFile throws on missing file", "[toml]") {
    REQUIRE_THROWS(anvil::toml::LoadFile("/nonexistent/path.toml"));
}


TEST_CASE("anvil::table Print does not throw with 2-row table", "[table][smoke]") {
    REQUIRE_NOTHROW(anvil::table::Print({
        {"col_a", "col_b"},
        {"val1",  "val2"},
    }));
}

ANVIL_TEST_CASE("anvil::test macro compiles", "[test][smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("anvil::progress Bar ticks and completes", "[progress][smoke]") {
    anvil::progress::Bar bar("test", 2);
    REQUIRE_NOTHROW(bar.Tick());
    REQUIRE_NOTHROW(bar.Tick());
    REQUIRE_NOTHROW(bar.Done());
}

ANVIL_PARITY_CASE("anvil::test parity macro compiles", "[test][smoke]", 1, 7, 16) {
    REQUIRE(n > 0);
}
ANVIL_PARITY_CASE_END

ANVIL_APPROX_CASE("anvil::test approx macro compiles", "[test][smoke]", 1e-6) {
    REQUIRE(tol < 1e-3);
}
ANVIL_APPROX_CASE_END
