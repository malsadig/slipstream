#include "slipstream/server/message_handler.hpp"
#include "slipstream/server/config.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    auto config = slipstream::server::parseConfig(argc, argv);

    if (config) {
        std::string_view symbol {
            config->symbol,
            sizeof(config->symbol)
        };
        symbol = symbol.substr(0, symbol.find('\0'));

        std::cout << "Symbol parsed: " << symbol << '\n';
        return EXIT_SUCCESS;
    }

    switch (config.error()) {
        case ConfigError::InvalidConfigCount:
            std::cerr << "Usage: slipstream --symbol <symbol>\n";
            break;

        case ConfigError::InvalidOption:
            std::cerr << "Expected the --symbol option.\n";
            break;

        case ConfigError::SymbolParseError:
            std::cerr << "Symbol must contain 1 to 12 ASCII bytes.\n";
            break;
    }

    return EXIT_FAILURE;
}
