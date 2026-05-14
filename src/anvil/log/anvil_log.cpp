#include <anvil/log/anvil_log.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdlib>
#include <string>

namespace anvil::log {

void Init(const std::string& tag) {
    if (spdlog::get(tag)) {
        spdlog::set_default_logger(spdlog::get(tag));
        return;
    }
    auto logger = spdlog::stdout_color_mt(tag);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%dT%H:%M:%S] [%^%l%$] [%n] %v");
    if (const char* env = std::getenv("ANVIL_LOG_LEVEL")) {
        std::string s{env};
        if      (s == "trace") SetLevel(Level::Trace);
        else if (s == "debug") SetLevel(Level::Debug);
        else if (s == "warn")  SetLevel(Level::Warn);
        else if (s == "error") SetLevel(Level::Error);
        else if (s == "off")   SetLevel(Level::Off);
        else                   SetLevel(Level::Info);
    }
}

void SetLevel(Level lvl) {
    switch (lvl) {
        case Level::Trace: spdlog::set_level(spdlog::level::trace); break;
        case Level::Debug: spdlog::set_level(spdlog::level::debug); break;
        case Level::Info:  spdlog::set_level(spdlog::level::info);  break;
        case Level::Warn:  spdlog::set_level(spdlog::level::warn);  break;
        case Level::Error: spdlog::set_level(spdlog::level::err);   break;
        case Level::Off:   spdlog::set_level(spdlog::level::off);   break;
    }
}

}  // namespace anvil::log
