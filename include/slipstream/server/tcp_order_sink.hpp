#pragma once

#include "slipstream/server/order_sink.hpp"
#include "slipstream/codec.hpp"

#include <array>
#include <cstdint>
#include <cerrno>
#include <stdexcept>
#include <chrono>
#include <cstddef>
#include <system_error>

#include <sys/socket.h>

namespace slipstream::server {
    class TcpOrderSink final : public IOrderSink {
    public:
        explicit TcpOrderSink(int fd) : fd_(fd) {}

        template<class T>
        void sendStamped(T message) {
            const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            if (now < 0) {
                throw std::runtime_error("Cannot encode OE timestamp");
            }

            message.ts_ns = static_cast<std::uint64_t>(now);

            std::array<std::byte, kNewOrderWireSize> frame{};
            const auto encoded = encode(message, frame);

            if (encoded.error != CodecError::NONE) {
                throw std::runtime_error("Cannot encode message");
            }

            std::size_t sent{};
            while (sent < encoded.bytes_written) {
                const auto result = ::send(
                    fd_,
                    frame.data() + sent,
                    encoded.bytes_written - sent,
                    MSG_DONTWAIT | MSG_NOSIGNAL
                );

                if (result > 0) {
                    sent += static_cast<std::size_t>(result);
                    continue;
                }

                if (result == -1) {
                    const int error = errno;
                    if (error == EINTR) {
                        continue;
                    }

                    if (error == EAGAIN || error == EWOULDBLOCK) {
                        throw std::runtime_error("OE output would block; stopping"); 
                    }
                    throw std::system_error(error, std::generic_category(), "OE Send");
                }

                throw std::runtime_error("OE send made no progress");
            }
        }

        void send(NewOrder order) override {
            sendStamped(order);
        }

        void send(SessionControl control) override {
            sendStamped(control);
        }

        void send(Heartbeat heartbeat) override {
            sendStamped(heartbeat);
        }
    private:
        int fd_;
    };
}