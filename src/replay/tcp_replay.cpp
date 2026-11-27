#include "slipstream/replay/tcp_replay.hpp"
#include "slipstream/replay/replay.hpp"
#include "slipstream/server/unique_fd.hpp"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace slipstream::replay {
    namespace {
        using slipstream::server::UniqueFd;

        std::uint64_t parseUnsigned(std::string_view text, const char* name) {
            std::uint64_t value{};

            const auto result = std::from_chars(
                text.data(), text.data() + text.size(), value
            );

            if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
                throw std::runtime_error(std::string{"Invalid "} + name);
            }

            return value;
        }

        UniqueFd connectTcp(const char* host, std::uint16_t port) {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_NUMERICSERV;

            const auto service = std::to_string(port);
            addrinfo* raw = nullptr;

            const int result = ::getaddrinfo(host, service.c_str(), &hints, &raw);

            if (result != 0) {
                if (result == EAI_SYSTEM) {
                    const int error = errno;
                    throw std::system_error(
                        error, std::generic_category(), "getaddrinfo"
                    );
                }

                throw std::runtime_error(std::string("getaddrinfo: ") + ::gai_strerror(result));
            }

            std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses {
                raw, &::freeaddrinfo
            };

            int last_error = EADDRNOTAVAIL;

            for (auto* address = addresses.get(); address != nullptr; address = address->ai_next) {
                UniqueFd connection {
                    ::socket(
                        address->ai_family,
                        address->ai_socktype,
                        address->ai_protocol
                    )
                };

                if (connection.get() == -1) {
                    last_error = errno;
                    continue;
                }

                if (::connect(connection.get(), address->ai_addr,
                    address->ai_addrlen) == 0) {
                    return connection;
                }

                last_error = errno;
            }

            throw std::system_error(last_error, std::generic_category(), "TCP connect");
        }

        class SocketBuffer final : public std::streambuf {
        public:
            explicit SocketBuffer(int fd) : fd_(fd) {}
        protected:
            std::streamsize xsputn(const char* data, std::streamsize count) override {
                std::streamsize sent{0};

                while (sent < count) {
                    const auto result = ::send(
                        fd_,
                        data + sent,
                        static_cast<std::size_t>(count - sent),
                        MSG_NOSIGNAL
                    );

                    if (result > 0) {
                        sent += result;
                        continue;
                    }

                    if (result == -1) {
                        const int error = errno;
                        if (error == EINTR) {
                            continue;
                        }

                        throw std::system_error(error, std::generic_category(), "TCP send");
                    }

                    throw std::runtime_error("TCP send made no progress");
                }

                return sent;
            }
            int sync() override {
                return 0;
            }
        private:
            int fd_;
        };
    }

    int runTcpReplay(int argc, char* argv[], MessageType selection) {
        try {
            if (argc != 7 || std::string_view{argv[5]} != "--start-at-ns") {
                throw std::runtime_error(
                    "Usage: <client> <input.csv> --tcp <host> <port> "
                    "--start-at-ns <future Unix nanoseconds>"
                );
            }

            if (std::string_view{argv[3]}.empty()) {
                throw std::runtime_error("Host must not be empty");
            }

            const auto port = parseUnsigned(argv[4], "port");
            if (port == 0 || port > 65535) {
                throw std::runtime_error("Port must be in 1-65535");
            }

            const auto start_ns = parseUnsigned(argv[6], "start time");

            std::ifstream input{argv[1]};
            if (!input) {
                throw std::runtime_error(
                    std::string{"Cannot open "} + argv[1]
                );
            }

            const auto rows = readCsvRows(input);
            const auto offsets = replayOffsetsNs(rows);
            const auto last_offset = offsets.empty() ? 0 : offsets.back();

            constexpr auto max_ns = static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()
            );

            if (start_ns > max_ns || last_offset > max_ns - start_ns) {
                throw std::runtime_error(
                    "Replay timestamps exceed the supported clock range"
                );
            }

            using Clock = std::chrono::system_clock;
            const auto start = Clock::time_point{
                std::chrono::duration_cast<Clock::duration>(
                    std::chrono::nanoseconds{
                        static_cast<std::int64_t>(start_ns)
                    }
                )
            };

            auto connection = connectTcp(
                argv[3], static_cast<std::uint16_t>(port)
            );

            if (Clock::now() >= start) {
                throw std::runtime_error(
                    "Replay start has passed; choose a later shared timestamp"
                );
            }

            SocketBuffer buffer{connection.get()};
            std::ostream output{&buffer};
            output.exceptions(std::ios::badbit | std::ios::failbit);

            const auto emitted =
                replayRows(rows, selection, output, start, true);

            std::cerr << "Sent " << emitted << " TCP frames" << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << argv[0] << ":" << error.what() << '\n';
            return 1;
        }
    }
}