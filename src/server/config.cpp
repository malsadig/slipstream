#include "slipstream/server/config.hpp"

#include <string_view>
#include <algorithm>

namespace slipstream::server {
    std::expected<ServerConfig, ConfigError> parseConfig(int argc, char* argv[]) {
        if (argc != 3) {
            return std::unexpected(ConfigError::InvalidConfigCount);
        }

        if (std::string_view{argv[1]} != "--symbol") {
            return std::unexpected(ConfigError::InvalidOption);
        }

        const std::string_view symbol{argv[2]};
        ServerConfig config{};

        if (symbol.empty() || symbol.size() > sizeof(config.symbol)) {
            return std::unexpected(ConfigError::SymbolParseError);
        }

        const bool is_ascii = std::all_of(
            symbol.begin(), symbol.end(),
            [](unsigned char byte) {
                return byte <= 0x7F;
            }
        );

        if (!is_ascii) {
            return std::unexpected(ConfigError::SymbolParseError);
        }

        std::copy(symbol.begin(), symbol.end(), config.symbol);

        return config;
    }
}