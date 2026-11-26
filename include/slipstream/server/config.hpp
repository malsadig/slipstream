#pragma once

#include <expected>

struct ServerConfig {
    char symbol[12]{};
};

enum class ConfigError {
    InvalidConfigCount,
    InvalidOption,
    SymbolParseError,
};

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]);
}