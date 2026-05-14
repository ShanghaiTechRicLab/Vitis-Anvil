#pragma once
#include <toml.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace anvil::toml {

using Table = ::toml::table;

inline Table LoadFile(const std::filesystem::path& p) {
    try {
        return ::toml::parse_file(p.string());
    } catch (const ::toml::parse_error& e) {
        throw std::runtime_error(std::string("anvil::toml: ") + e.what());
    }
}

template <typename T>
T Require(const Table& t, std::string_view key) {
    auto node = t[key];
    if (!node) throw std::runtime_error(
        "anvil::toml: required key '" + std::string(key) + "' not found");
    auto val = node.value<T>();
    if (!val) throw std::runtime_error(
        "anvil::toml: key '" + std::string(key) + "' has wrong type");
    return *val;
}

}  // namespace anvil::toml
