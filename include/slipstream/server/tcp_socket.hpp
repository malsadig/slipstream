#pragma once

#include "slipstream/server/unique_fd.hpp"
#include "slipstream/server/config.hpp"

namespace slipstream::server {
    [[nodiscard]] UniqueFd createTcpSocket();

    void bindTcpSocket(const UniqueFd& socket, const EndpointConfig& endpoint);

    void listenTcpSocket(const UniqueFd& socket);

    [[nodiscard]] UniqueFd acceptTcpClient(const UniqueFd& listener);
}