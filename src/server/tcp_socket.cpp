#include "slipstream/server/tcp_socket.hpp"

#include <cerrno>
#include <system_error>
#include <netinet/in.h>
#include <sys/socket.h>
#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <string>

namespace slipstream::server {
    UniqueFd createTcpSocket() {
        const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (fd == -1) {
            const int error = errno;
            throw std::system_error(error, std::generic_category(), "socket");
        }

        return UniqueFd{fd};
    }

    void bindTcpSocket(const UniqueFd& socket, const EndpointConfig& endpoint) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_NUMERICSERV;

        const std::string service = std::to_string(endpoint.port);
        addrinfo* raw = nullptr;

        const int result = ::getaddrinfo(endpoint.host.c_str(), service.c_str(), &hints, &raw);

        if (result != 0) {
            if (result == EAI_SYSTEM) {
                const int error = errno;
                throw std::system_error(error, std::generic_category(), "getaddrinfo");
            }

            throw std::runtime_error(std::string{"getaddrinfo: "} + ::gai_strerror(result));
        }

        std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses{raw, &::freeaddrinfo};

        int lastError = EADDRNOTAVAIL;

        for (auto* address = addresses.get(); address != nullptr; address = address->ai_next) {
            if (::bind(socket.get(), address->ai_addr, address->ai_addrlen) == 0) {
                return;
            }
            lastError = errno;
        }

        throw std::system_error(lastError, std::generic_category(), "bind");
    }
}