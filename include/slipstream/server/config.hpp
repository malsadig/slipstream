#pragma once

#include <expected>
#include <cstdint>
#include <cstddef>
#include <string>

struct EndpointConfig {
    std::string host;
    std::uint16_t port{};
};

enum class Transport {
    TCP
};

struct ServerConfig {
    char symbol[12]{};
    std::uint64_t vwap_window_ns{};
    std::size_t vwap_capacity{4096};
    std::size_t report_capacity{4096};
    EndpointConfig md{};
    EndpointConfig oe{};
    Transport transport{Transport::TCP};
};

enum class ConfigError {
    InvalidConfigCount,
    InvalidOption,
    SymbolParseError,
    WindowParseError,
    VwapCapacityParseError,
    ReportCapacityParseError,
    HostParseError,
    PortParseError,
    TransportParseError,
};

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]);
}