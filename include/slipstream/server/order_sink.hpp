#pragma once

#include "slipstream/messages.hpp"

namespace slipstream::server {
    class IOrderSink {
    public:
        virtual ~IOrderSink() = default;
        virtual void send(NewOrder order) = 0;
        virtual void send(SessionControl control) = 0;
    };
}