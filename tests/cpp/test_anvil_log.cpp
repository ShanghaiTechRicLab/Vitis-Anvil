#include <catch_amalgamated.hpp>
#include <anvil/log/anvil_log.hpp>
#include <anvil/cli/anvil_cli.hpp>
#include <anvil/json/anvil_json.hpp>
#include <anvil/toml/anvil_toml.hpp>

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
