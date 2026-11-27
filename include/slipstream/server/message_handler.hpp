#pragma once

#include "slipstream/codec.hpp"

namespace slipstream::server {
    enum class FeedRole {
        MarketData,
        OrderEntry,
        Console,
    };

    class IMessageHandler {
    public:
        virtual ~IMessageHandler() = default;

        // The transport retains ownership of message; it is borrowed for this call.
        // Fatal input/processing errors throw; the run owner applies its stop policy.
        virtual void onMessage(FeedRole role, const Message& message) = 0;
    };
}
