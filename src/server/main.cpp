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
        std::cout << "Window (ns): " << config->vwap_window_ns << '\n';
        std::cout << "VWAP capacity: " << config->vwap_capacity << '\n';
        std::cout << "Report capacity: " << config->report_capacity << '\n';
        std::cout << "MD endpoint: " << config->md.host << ':' << config->md.port << '\n';
        std::cout << "OE endpoint: " << config->oe.host << ':' << config->oe.port << '\n';
        return EXIT_SUCCESS;
    }

    switch (config.error()) {
        case ConfigError::InvalidConfigCount:
            std::cerr << "Usage: slipstream --symbol <symbol> "
                "--vwap-window-ms <positive integer> "
                "--md-host <host> --md-port <port> "
                "--oe-host <host> --oe-port <port> "
                "--transport tcp "
                "[--vwap-capacity <entries>] "
                "[--report-capacity <entries>]" << '\n';
            break;

        case ConfigError::InvalidOption:
            std::cerr << "Unknown or repeated option." << '\n';
            break;

        case ConfigError::SymbolParseError:
            std::cerr << "Symbol must contain 1 to 12 ASCII bytes" << '\n';
            break;
        case ConfigError::WindowParseError:
            std::cerr << "Window must be a positive integer in milliseconds "
                "that fits after conversion to nanoseconds." << '\n';
            break;
        case ConfigError::VwapCapacityParseError:
            std::cerr << "VWAP capacity must be a positive integer that fits in std::size_t." << '\n';
            break;
        case ConfigError::ReportCapacityParseError:
            std::cerr << "Report capacity must be a positive integer that fits in std::size_t." << '\n';
            break;
        case ConfigError::HostParseError:
            std::cerr << "Host must not be empty.\n";
            break;
        case ConfigError::PortParseError:
            std::cerr << "Port must be an integer from 1 to 65535.\n";
            break;
        case ConfigError::TransportParseError:
            std::cerr << "Transport must be tcp for M2.\n";
            break;
    }

    return EXIT_FAILURE;
}
