#pragma once

#include "slipstream/server/message_handler.hpp"
#include "slipstream/server/unique_fd.hpp"
#include "slipstream/server/order_sink.hpp"

namespace slipstream::server {
    void runTcpFeeds(const UniqueFd& md, const UniqueFd& oe,
        IMessageHandler& handler, int failure_fd, IOrderSink& orders);
}