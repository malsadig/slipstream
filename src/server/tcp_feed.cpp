#include "slipstream/server/tcp_feed.hpp"
#include "slipstream/stream_decoder.hpp"
#include "slipstream/server/order_sink.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <poll.h>
#include <sys/socket.h>

#include <iostream>
#include <string_view>
#include <unistd.h>

#include <chrono>

namespace slipstream::server {
    namespace {
        [[noreturn]] void fail(const char* feed, const char* reason) {
            throw std::runtime_error(std::string{feed} + ": " + reason);
        }

        class ConsoleInput {
        public:
            int fd() const noexcept {
                return fd_;
            }

            bool consume(short events, IMessageHandler& handler) {
                if (events & POLLNVAL) {
                    fd_ = -1;
                    return false;
                }

                if (events & POLLERR) {
                    throw std::runtime_error("Console input error");
                }

                if (!(events & (POLLIN | POLLHUP))) {
                    return false;
                }

                std::array<char, 128> bytes{};
                const auto count = ::read(fd_, bytes.data(), bytes.size());

                if (count == -1) {
                    const int error = errno;
                    if (error == EINTR || error == EAGAIN || error == EWOULDBLOCK) {
                        return false;
                    }

                    throw std::system_error(error, std::generic_category(), "Console read");
                }
                
                if (count == 0) {
                    fd_ = -1;
                    return finishLine(handler);
                }

                for (auto i{0uz}; i < static_cast<std::size_t>(count); ++i) {
                    const char c = bytes[i];

                    if (c == '\n') {
                        if (finishLine(handler)) {
                            return true;
                        }
                    } else if (!overlong_) {
                        if (used_ == line_.size()) {
                            overlong_ = true;
                        } else {
                            line_[used_++] = c;
                        }
                    }
                }
                return false;
            }
        private:
            bool finishLine(IMessageHandler& handler) {
                if (overlong_) {
                    used_ = 0;
                    overlong_ = false;
                    std::cerr << "Console command too long" << '\n';
                    return false;
                }

                std::string_view command{line_.data(), used_};

                used_ = 0;

                if (!command.empty() && command.back() == '\r') {
                    command.remove_suffix(1);
                }
                if (command.empty()) {
                    return false;
                }

                SessionControl control{};
                if (command == "HALT") {
                    control.state = SessionState::HALT;
                } else if (command == "OPEN") {
                    control.state = SessionState::OPEN;
                } else if (command == "CLOSE") {
                    control.state = SessionState::CLOSE;
                } else {
                    std::cerr << "Commands: HALT, OPEN, or CLOSE" << '\n';
                    return false;
                }

                handler.onMessage(FeedRole::Console, Message{control});
                return control.state == SessionState::CLOSE;
            }

            int fd_{STDIN_FILENO};
            std::array<char, 64> line_{};
            std::size_t used_{};
            bool overlong_{};
        };
    }

    void runTcpFeeds(const UniqueFd& md, const UniqueFd& oe,
        IMessageHandler& handler, int failure_fd, IOrderSink& orders) {
        ConsoleInput console;

        std::array<pollfd, 4> sockets {{
            {md.get(), POLLIN, 0},
            {oe.get(), POLLIN, 0},
            {failure_fd, POLLIN, 0},
            {console.fd(), POLLIN, 0},
        }};

        std::array<StreamDecoder, 2> decoders{};

        constexpr std::array roles {
            FeedRole::MarketData, FeedRole::OrderEntry
        };

        constexpr std::array names{"MD", "OE"};

        std::array<std::byte, 4096> buffer{};

        using Clock = std::chrono::steady_clock;
        constexpr auto stale_interval = std::chrono::seconds{5};
        auto next_stale = Clock::now() + stale_interval;

        for (;;) {
            const auto now = Clock::now();
            const int timeout = now >= next_stale ? 0 : 
                static_cast<int>(
                    std::chrono::ceil<std::chrono::milliseconds>(
                        next_stale - now
                    ).count()
                );

            if (::poll(sockets.data(), sockets.size(), timeout) == -1) {
                const int error = errno;
                if (error == EINTR) {
                    continue;
                }

                throw std::system_error(error, std::generic_category(), "poll");
            }

            if (sockets[2].revents != 0) {
                fail("Reporter", "output failure or invalid control descriptor");
            }

            if (console.consume(sockets[3].revents, handler)) {
                return; // CLOSE: existing owner cleans up sockets and reporter.
            }
            sockets[3].fd = console.fd();

            for (auto i{0uz}; i < decoders.size(); ++i) {
                const short events = sockets[i].revents;

                if (events & POLLNVAL) {
                    fail(names[i], "invalid socket descriptor");
                }

                if (!(events & (POLLIN | POLLHUP | POLLERR))) {
                    continue;
                }

                ssize_t received;
                do {
                    received = ::recv(
                        sockets[i].fd,
                        buffer.data(),
                        buffer.size(),
                        MSG_DONTWAIT
                    );
                } while (received == -1 && errno == EINTR);

                if (received == -1) {
                    const int error = errno;
                    if (error == EAGAIN || error == EWOULDBLOCK) {
                        if (events & POLLERR) {
                            fail(names[i], "socket error without readable data");
                        }
                        continue;
                    }

                    throw std::system_error(
                        error, std::generic_category(),
                        std::string{names[i]} + " recv"
                    );
                }

                if (received == 0) {
                    if (decoders[i].finish() != CodecError::NONE) {
                        fail(names[i], "incomplete frame at EOF");
                    }
                    return;
                }

                const auto received_at = Clock::now();

                auto result = decoders[i].feed(
                    std::span<const std::byte> {
                        buffer.data(),
                        static_cast<std::size_t>(received)
                    }
                );

                for (const auto& message : result.messages) {
                    if (roles[i] == FeedRole::OrderEntry) {
                        next_stale = received_at + stale_interval;
                    }

                    handler.onMessage(roles[i], message);
                }

                if (result.error != CodecError::NONE) {
                    fail(names[i], "invalid frame");
                }
            }

            if (Clock::now() >= next_stale) {
                std::cerr << "OE stale: no message for at least 5 seconds\n";

                orders.send(Heartbeat{});

                next_stale = Clock::now() + stale_interval;
            }
        }
    }
}