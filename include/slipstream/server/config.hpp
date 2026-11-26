#pragma once

#include <expected>
#include <cstdint>
#include <cstddef>

struct ServerConfig {
    char symbol[12]{};
    std::uint64_t vwap_window_ns{};
    std::size_t vwap_capacity{4096};
    std::size_t report_capacity{4096};
};

enum class ConfigError {
    InvalidConfigCount,
    InvalidOption,
    SymbolParseError,
    WindowParseError,
    VwapCapacityParseError,
    ReportCapacityParseError,
};

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]);
}