#include "slipstream/replay/tcp_replay.hpp"
#include "slipstream/replay/replay.hpp"
#include "slipstream/server/unique_fd.hpp"
#include "slipstream/stream_decoder.hpp"

#include <array>
#include <span>
#include <variant>

#include <poll.h>

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
        
        class TcpResponseReader {
        public:
            explicit TcpResponseReader(int fd) : fd_(fd) {}

            bool waitUntil(std::chrono::steady_clock::time_point deadline) {
                while (!closed_) {
                    const auto now = std::chrono::steady_clock::now();
                    int timeout = 0;
                    if (deadline > now) {
                        const auto ms = std::chrono::ceil<std::chrono::milliseconds>(
                            deadline - now
                        ).count();

                        timeout = ms > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : static_cast<int>(ms);
                    }

                    pump(timeout);

                    if (closed_) {
                        return false;
                    }

                    if (std::chrono::steady_clock::now() >= deadline) {
                        return true;
                    }
                }

                return false;
            }

            void finish() {
                if (closed_) {
                    return;
                }

                if (::shutdown(fd_, SHUT_WR) == -1 && errno != ENOTCONN) {
                    const int error = errno;
                    throw std::system_error(
                        error, std::generic_category(), "OE shutdown"
                    );
                }

                while (!closed_) {
                    pump(-1);
                }
            }
        private:
            void pump(int timeout) {
                pollfd socket{fd_, POLLIN, 0};
                const int ready = ::poll(&socket, 1, timeout);

                if (ready == -1) {
                    const int error = errno;
                    if (error == EINTR) {
                        return;
                    }
                    throw std::system_error(error, std::generic_category(), "OE response poll");
                }

                if (ready == 0) {
                    return;
                }
                if (socket.revents & POLLNVAL) {
                    throw std::runtime_error("Invalid OE response socket");
                }
                if (!(socket.revents & (POLLIN | POLLHUP | POLLERR))) {
                    return;
                }

                std::array<std::byte, 4096> bytes{};
                ssize_t received;

                do {
                    received = ::recv(
                        fd_, bytes.data(), bytes.size(), MSG_DONTWAIT
                    );
                } while (received == -1 && errno == EINTR);

                if (received == -1) {
                    const int error = errno;

                    if ((error == EAGAIN || error == EWOULDBLOCK) && !(socket.revents & POLLERR)) {
                        return;
                    }

                    throw std::system_error(error, std::generic_category(), "OE response recv");
                }

                if (received == 0) {
                    if (decoder_.finish() != CodecError::NONE) {
                        throw std::runtime_error("Incomplete OE response at EOF");
                    }
                    closed_ = true;
                    return;
                }

                auto result = decoder_.feed(
                    std::span<const std::byte>{
                        bytes.data(), static_cast<std::size_t>(received)
                    }
                );

                for (const auto& message : result.messages) {
                    const auto* order = std::get_if<NewOrder>(&message);
                    if (!order) {
                        throw std::runtime_error("Expected a NewOrder response");
                    }

                    std::string_view symbol{order->symbol, kSymbolSize};
                    symbol = symbol.substr(0, symbol.find('\0'));

                    std::cout << "[ORDER] id=" << order->client_order_id
                        << " trade_id=" << order->trade_id
                        << " symbol=" << symbol
                        << " status=" << static_cast<char>(order->status)
                        << " side=" << static_cast<char>(order->side)
                        << " qty=" << order->qty
                        << " limit_px_x10000=" << order->limit_px
                        << " ts_ns=" << order->ts_ns << '\n';

                    std::cout.flush();
                    if (!std::cout) {
                        throw::std::runtime_error("Cannot print OE response");
                    }
                }

                if (result.error != CodecError::NONE) {
                    throw std::runtime_error("Invalid OE response");
                }
            }

            int fd_;
            StreamDecoder decoder_;
            bool closed_{};
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

            std::size_t emitted{};

            if (selection == MessageType::TRADE) {
                TcpResponseReader responses{connection.get()};

                emitted = replayRows(
                    rows, selection, output, start, true,
                    [&responses](std::chrono::steady_clock::time_point deadline) {
                        return responses.waitUntil(deadline);
                    }
                );

                responses.finish();
            } else {
                emitted = replayRows(rows, selection, output, start, true);
            }

            std::cerr << "Sent " << emitted << " TCP frames" << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << argv[0] << ":" << error.what() << '\n';
            return 1;
        }
    }
}