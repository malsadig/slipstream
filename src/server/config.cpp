#include "slipstream/server/config.hpp"

#include <string_view>
#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]) {
        if (argc < 3 || (argc - 1) % 2 != 0) {
            return std::unexpected(ConfigError::InvalidConfigCount);
        }

        ServerConfig config{};
        bool parsedSymbol{false};
        bool parsedWindow{false};

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
            } else {
                return std::unexpected(ConfigError::InvalidOption);
            }
        }

        if (!parsedSymbol || !parsedWindow) {
            return std::unexpected(ConfigError::InvalidConfigCount);
        }

        return config;
    }
}