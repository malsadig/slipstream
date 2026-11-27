#include "slipstream/server/message_handler.hpp"
#include "slipstream/server/config.hpp"
#include "slipstream/server/unique_fd.hpp"
#include "slipstream/server/tcp_socket.hpp"
#include "slipstream/server/l1_handler.hpp"
#include "slipstream/server/tcp_feed.hpp"
#include "slipstream/server/async_reporter.hpp"

#include <cerrno>
#include <stdexcept>
#include <signal.h>

#include <system_error>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <exception>

int main(int argc, char* argv[]) {
    std::cerr.tie(nullptr);

    using namespace slipstream::server;

    auto config = parseConfig(argc, argv);

    if (config) {
        std::string_view symbol {
            config->symbol,
            sizeof(config->symbol)
        };
        symbol = symbol.substr(0, symbol.find('\0'));

        std::cerr << "Symbol parsed: " << symbol << '\n';
        std::cerr << "Window (ns): " << config->vwap_window_ns << '\n';
        std::cerr << "VWAP capacity: " << config->vwap_capacity << '\n';
        std::cerr << "Report capacity: " << config->report_capacity << '\n';
        std::cerr << "MD endpoint: " << config->md.host << ':' << config->md.port << '\n';
        std::cerr << "OE endpoint: " << config->oe.host << ':' << config->oe.port << '\n';
        
        try {
            struct sigaction action{};
            action.sa_handler = SIG_IGN;
            ::sigemptyset(&action.sa_mask);

            if (::sigaction(SIGPIPE, &action, nullptr) == -1) {
                const int error = errno;
                throw std::system_error(
                    error, std::generic_category(), "sigaction(SIGPIPE)"
                );
            }

            AsyncReporter reporter{config->report_capacity, std::cout};
            std::exception_ptr processing_error;

            try {
                L1Handler handler{
                    symbol,
                    config->vwap_window_ns,
                    config->vwap_capacity,
                    &reporter
                };
                
                auto mdListener = createTcpSocket();
                bindTcpSocket(mdListener, config->md);
                listenTcpSocket(mdListener);

                auto oeListener = createTcpSocket();
                bindTcpSocket(oeListener, config->oe);
                listenTcpSocket(oeListener);

                std::cerr << "MD listening on "
                    << config->md.host << ':' << config->md.port << '\n'
                    << "OE listening on "
                    << config->oe.host << ':' << config->oe.port << '\n'
                    << "Waiting for both clients...\n";
                
                auto mdConnection = acceptTcpClient(mdListener);

                std::cerr << "MD accepted (fd=" << mdConnection.get()
                    << ")\n";

                auto oeConnection = acceptTcpClient(oeListener);

                std::cerr << "OE accepted (fd=" << oeConnection.get()
                    << ")\n";

                runTcpFeeds(mdConnection, oeConnection, handler, reporter.failureFd());
            } catch (...) {
                processing_error = std::current_exception();
            }

            const bool output_ok = reporter.finish();

            if (processing_error) {
                std::rethrow_exception(processing_error);
            }

            if (!output_ok) {
                throw std::runtime_error("Report printer failed");
            }

            std::cerr << "Feeds ended cleanly." << '\n';
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "Server failed: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
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
