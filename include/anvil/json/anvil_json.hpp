#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace anvil::json {

using Value = nlohmann::json;

inline Value LoadFile(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) throw std::runtime_error("anvil::json: cannot open " + p.string());
    return nlohmann::json::parse(f);
}

inline void DumpFile(const std::filesystem::path& p, const Value& v, int indent = 2) {
    std::ofstream f(p);
    if (!f) throw std::runtime_error("anvil::json: cannot write " + p.string());
    f << v.dump(indent) << "\n";
}

}  // namespace anvil::json
