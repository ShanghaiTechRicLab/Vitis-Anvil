#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <utility>

namespace anvil::log {

enum class Level { Trace, Debug, Info, Warn, Error, Off };

void Init(const std::string& tag = "anvil");
void SetLevel(Level lvl);

template <typename... Args>
void Trace(fmt::format_string<Args...> fmt, Args&&... args) {
    spdlog::trace(fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void Debug(fmt::format_string<Args...> fmt, Args&&... args) {
    spdlog::debug(fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void Info(fmt::format_string<Args...> fmt, Args&&... args) {
    spdlog::info(fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void Warn(fmt::format_string<Args...> fmt, Args&&... args) {
    spdlog::warn(fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void Error(fmt::format_string<Args...> fmt, Args&&... args) {
    spdlog::error(fmt, std::forward<Args>(args)...);
}

}  // namespace anvil::log
