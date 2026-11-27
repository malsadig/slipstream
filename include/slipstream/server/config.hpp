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

    std::uint32_t max_quantity{};
    std::uint32_t band_bps_x10000{};
    std::uint32_t participation_cap_x10000{};
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
    
    MaxQuantityParseError,
    BandParseError,
    ParticipationCapParseError,
};

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]);
}