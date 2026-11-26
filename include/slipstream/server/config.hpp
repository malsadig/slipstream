#pragma once

#include <expected>
#include <cstdint>

struct ServerConfig {
    char symbol[12]{};
    std::uint64_t vwap_window_ns{};
};

enum class ConfigError {
    InvalidConfigCount,
    InvalidOption,
    SymbolParseError,
    WindowParseError,
};

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]);
}