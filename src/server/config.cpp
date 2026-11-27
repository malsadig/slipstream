#include "slipstream/server/config.hpp"

#include <string_view>
#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>
#include <optional>

namespace slipstream::server {
    namespace {
        std::optional<std::uint32_t> parseDecimal4(
            std::string_view text, std::uint32_t max_scaled) {
            if (text.empty() || text.front() == '.' || text.back() == '.') {
                return std::nullopt;
            }

            std::uint64_t whole{};
            std::uint32_t fraction{};
            unsigned fractional_digits{};
            bool after_point{false};

            for (const char ch : text) {
                if (ch == '.' && !after_point) {
                    after_point = true;
                    continue;
                }

                if (ch < '0' || ch > '9') {
                    return std::nullopt;
                }

                const auto digit = static_cast<std::uint32_t>(ch - '0');

                if (!after_point) {
                    whole = whole * 10 + digit;
                    if (whole > max_scaled / 10'000) {
                        return std::nullopt;
                    }
                } else {
                    if (++fractional_digits > 4) {
                        return std::nullopt;
                    }
                    fraction = fraction * 10 + digit;
                }
            }

            while (fractional_digits < 4) {
                fraction *= 10;
                ++fractional_digits;
            }

            const auto scaled = whole * 10'000 + fraction;
            if (scaled > max_scaled) {
                return std::nullopt;
            }

            return static_cast<std::uint32_t>(scaled);
        }
    }

    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]) {
        if (argc < 3 || (argc - 1) % 2 != 0) {
            return std::unexpected(ConfigError::InvalidConfigCount);
        }

        ServerConfig config{};

        bool parsedSymbol{false};
        bool parsedWindow{false};
        bool parsedVwapCapacity{false};
        bool parsedReportCapacity{false};

        bool parsedMdHost{false};
        bool parsedMdPort{false};
        bool parsedOeHost{false};
        bool parsedOePort{false};

        bool parsedTransport{false};

        bool parsedMaxQuantity{false};
        bool parsedBand{false};
        bool parsedParticipation{false};

        for (int i = 1; i < argc; i += 2) {
            const std::string_view option{argv[i]};
            const std::string_view value{argv[i + 1]};

            if (option == "--symbol") {
                if (parsedSymbol) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                if (value.empty() || value.size() > sizeof(config.symbol)) {
                    return std::unexpected(ConfigError::SymbolParseError);
                }

                const bool is_ascii = std::all_of(
                    value.begin(), value.end(),
                    [](unsigned char byte) {
                        return byte <= 0x7F;
                    }
                );

                if (!is_ascii) {
                    return std::unexpected(ConfigError::SymbolParseError);
                }

                std::copy(value.begin(), value.end(), config.symbol);

                parsedSymbol = true;
            } else if (option == "--vwap-window-ms") {
                if (parsedWindow) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                std::uint64_t milliseconds{};
                const auto end = value.data() + value.size();
                const auto [ptr, error] = std::from_chars(value.data(), end, milliseconds);

                constexpr std::uint64_t nsPerMs = 1'000'000;
                constexpr auto maxMillis = std::numeric_limits<std::uint64_t>::max() / nsPerMs;

                if (error != std::errc{} || ptr != end || milliseconds == 0 || milliseconds > maxMillis) {
                    return std::unexpected(ConfigError::WindowParseError);
                }

                config.vwap_window_ns = milliseconds * nsPerMs;

                parsedWindow = true;
            } else if (option == "--vwap-capacity") {
                if (parsedVwapCapacity) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                std::size_t capacity{};

                const auto end = value.data() + value.size();
                const auto [ptr, error] =
                    std::from_chars(value.data(), end, capacity);

                if (error != std::errc{} || ptr != end || capacity == 0) {
                    return std::unexpected(ConfigError::VwapCapacityParseError);
                }

                config.vwap_capacity = capacity;
                parsedVwapCapacity = true;
            } else if (option == "--report-capacity") {
                if (parsedReportCapacity) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                std::size_t capacity{};

                const auto end = value.data() + value.size();
                const auto [ptr, error] = std::from_chars(value.data(), end, capacity);

                if (error != std::errc{} || ptr != end || capacity == 0) {
                    return std::unexpected(ConfigError::ReportCapacityParseError);
                }

                config.report_capacity = capacity;
                parsedReportCapacity = true;
            } else if (option == "--md-host" || option == "--oe-host") {
                const bool isMd = option == "--md-host";
                auto& endpoint = isMd ? config.md : config.oe;
                auto& parsed = isMd ? parsedMdHost : parsedOeHost;

                if (parsed) {
                    return std::unexpected(ConfigError::InvalidOption);
                }
                if (value.empty()) {
                    return std::unexpected(ConfigError::HostParseError);
                }

                endpoint.host = value;
                parsed = true;
            } else if (option == "--md-port" || option == "--oe-port") {
                const bool isMd = option == "--md-port";
                auto& endpoint = isMd ? config.md : config.oe;
                auto& parsed = isMd ? parsedMdPort : parsedOePort;

                if (parsed) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                std::uint16_t port{};

                const auto end = value.data() + value.size();
                const auto [ptr, error] =
                    std::from_chars(value.data(), end, port);

                if (error != std::errc{} || ptr != end || port == 0) {
                    return std::unexpected(ConfigError::PortParseError);
                }

                endpoint.port = port;
                parsed = true;
            } else if (option == "--transport") {
                if (parsedTransport) {
                    return std::unexpected(ConfigError::InvalidOption);
                }
                if (value != "tcp") {
                    return std::unexpected(ConfigError::TransportParseError);
                }

                config.transport = Transport::TCP;
                parsedTransport = true;
            } else if (option == "--max-quantity") {
                if (parsedMaxQuantity) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                std::uint32_t quantity{};
                const auto end = value.data() + value.size();
                const auto [ptr, error] =
                    std::from_chars(value.data(), end, quantity);

                if (error != std::errc{} || ptr != end || quantity == 0) {
                    return std::unexpected(ConfigError::MaxQuantityParseError);
                }

                config.max_quantity = quantity;
                parsedMaxQuantity = true;

            } else if (option == "--band-bps" || option == "--participation-cap") {
                const bool is_band = option == "--band-bps";
                auto& parsed = is_band ? parsedBand : parsedParticipation;

                if (parsed) {
                    return std::unexpected(ConfigError::InvalidOption);
                }

                const auto number = parseDecimal4(
                    value, is_band ? 100'000'000U : 10'000U
                );

                if (!number) {
                    return std::unexpected(
                        is_band ? ConfigError::BandParseError
                            : ConfigError::ParticipationCapParseError
                    );
                }

                if (is_band) {
                    config.band_bps_x10000 = *number;
                } else {
                    config.participation_cap_x10000 = *number;
                }

                parsed = true;
            } else {
                return std::unexpected(ConfigError::InvalidOption);
            }
        }

        if (!parsedSymbol || !parsedWindow || !parsedMdHost || !parsedMdPort ||
            !parsedOeHost || !parsedOePort || !parsedTransport ||
            !parsedBand || !parsedParticipation || !parsedMaxQuantity) {
            return std::unexpected(ConfigError::InvalidConfigCount);
        }

        return config;
    }
}