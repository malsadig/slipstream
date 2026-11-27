#include "slipstream/server/tcp_feed.hpp"
#include "slipstream/stream_decoder.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <poll.h>
#include <sys/socket.h>

namespace slipstream::server {
    namespace {
        [[noreturn]] void fail(const char* feed, const char* reason) {
            throw std::runtime_error(std::string{feed} + ": " + reason);
        }
    }

    void runTcpFeeds(const UniqueFd& md, const UniqueFd& oe, IMessageHandler& handler, int failure_fd) {
        std::array<pollfd, 3> sockets {{
            {md.get(), POLLIN, 0},
            {oe.get(), POLLIN, 0},
            {failure_fd, POLLIN, 0},
        }};

        std::array<StreamDecoder, 2> decoders{};

        constexpr std::array roles {
            FeedRole::MarketData, FeedRole::OrderEntry
        };

        constexpr std::array names{"MD", "OE"};

        std::array<std::byte, 4096> buffer{};

        for (;;) {
            if (::poll(sockets.data(), sockets.size(), -1) == -1) {
                const int error = errno;
                if (error == EINTR) {
                    continue;
                }

                throw std::system_error(error, std::generic_category(), "poll");
            }

            if (sockets[2].revents != 0) {
                fail("Reporter", "output failure or invalid control descriptor");
            }

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

                auto result = decoders[i].feed(
                    std::span<const std::byte> {
                        buffer.data(),
                        static_cast<std::size_t>(received)
                    }
                );

                for (const auto& message : result.messages) {
                    handler.onMessage(roles[i], message);
                }

                if (result.error != CodecError::NONE) {
                    fail(names[i], "invalid frame");
                }
            }
        }
    }
}