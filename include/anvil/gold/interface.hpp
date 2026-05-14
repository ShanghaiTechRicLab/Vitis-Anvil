#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

namespace anvil::gold {

template <typename T>
std::vector<T> LoadVector(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("anvil::gold: cannot open " + p.string());
    auto bytes = f.tellg();
    if (bytes < 0 || bytes % static_cast<std::streamoff>(sizeof(T)) != 0) {
        throw std::runtime_error("anvil::gold: file size not multiple of element size");
    }
    std::vector<T> v(static_cast<std::size_t>(bytes) / sizeof(T));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(v.data()), bytes);
    if (!f) throw std::runtime_error("anvil::gold: failed to read " + p.string());
    return v;
}

template <typename T>
void DumpVector(const std::filesystem::path& p, std::span<const T> v) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("anvil::gold: cannot write " + p.string());
    f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(T)));
    f.flush();
    if (!f) throw std::runtime_error("anvil::gold: failed to write " + p.string());
}

template <typename InT, typename OutT>
using GoldFn = std::function<void(std::span<const InT>, std::span<OutT>)>;

}  // namespace anvil::gold
