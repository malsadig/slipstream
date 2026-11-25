#pragma once

#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"

#include <array>

namespace fixtures {
    constexpr std::uint64_t timestamp = 0x0102030405060708ULL;

    constexpr char expectedSymbol[kSymbolSize] = "somesymbol";

    constexpr std::array<std::byte, kHeartbeatWireSize> heartbeatBytes{
        std::byte{0x08}, std::byte{0x00},
        std::byte{0x03}, std::byte{0x01},
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01}
    };

    constexpr std::array<std::byte, kQuoteWireSize> quoteBytes{
        // Header: body length = 44, type = Quote, version = 1
        std::byte{0x2C}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x01},

        // Symbol: "somesymbol" + two null bytes
        std::byte{0x73}, std::byte{0x6F}, // s o
        std::byte{0x6D}, std::byte{0x65}, // m e
        std::byte{0x73}, std::byte{0x79}, // s y
        std::byte{0x6D}, std::byte{0x62}, // m b
        std::byte{0x6F}, std::byte{0x6C}, // o l
        std::byte{0x00}, std::byte{0x00},

        // Timestamp: 0x0102030405060708
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01},

        // bid_qty: 0
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // bid_px: 0
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // ask_qty: 100 = 0x64
        std::byte{0x64}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // ask_px: 100000 = 0x186A0
        std::byte{0xA0}, std::byte{0x86},
        std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}
    };

    constexpr std::array<std::byte, kTradeWireSize> tradeBytes{
        // Header: body length = 41, type = Trade, version = 1
        std::byte{0x29}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x01},

        // Symbol: "somesymbol" + two null bytes
        std::byte{0x73}, std::byte{0x6F}, // s o
        std::byte{0x6D}, std::byte{0x65}, // m e
        std::byte{0x73}, std::byte{0x79}, // s y
        std::byte{0x6D}, std::byte{0x62}, // m b
        std::byte{0x6F}, std::byte{0x6C}, // o l
        std::byte{0x00}, std::byte{0x00},

        // Timestamp: 0x0102030405060708
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01},

        // qty: 100 = 0x64
        std::byte{0x64}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // px: 100000 = 0x186A0
        std::byte{0xA0}, std::byte{0x86},
        std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // aggressor: 'B' = ASCII 0x42
        std::byte{0x42},

        // id: 42 = 0x2A
        std::byte{0x2A}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}
    };

    constexpr std::array<std::byte, kSessionControlWireSize> sessionControlBytes{
        // Header: body length = 9, type = SessionControl, version = 1
        std::byte{0x09}, std::byte{0x00},
        std::byte{0x04}, std::byte{0x01},

        // Timestamp: 0x0102030405060708
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01},

        // state: HALT = 1
        std::byte{0x01}
    };

    constexpr std::array<std::byte, kNewOrderWireSize> newOrderBytes {
        // Header: body length = 50, type = New Order, version = 1
        std::byte{0x32}, std::byte{0x00},
        std::byte{0x0A}, std::byte{0x01},

        // Client Order Id: 0x1112131415161718ULL
        std::byte{0x18}, std::byte{0x17},
        std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13},
        std::byte{0x12}, std::byte{0x11},

        // Symbol: "somesymbol" + two null bytes
        std::byte{0x73}, std::byte{0x6F}, // s o
        std::byte{0x6D}, std::byte{0x65}, // m e
        std::byte{0x73}, std::byte{0x79}, // s y
        std::byte{0x6D}, std::byte{0x62}, // m b
        std::byte{0x6F}, std::byte{0x6C}, // o l
        std::byte{0x00}, std::byte{0x00},

        // Status: ACCEPTED 'A' = ASCII 0x41
        std::byte{0x41},

        // Timestamp: 0x0102030405060708
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01},

        // trade_id: 42 = 0x2A
        std::byte{0x2A}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // Side: SELL 'S' = ASCII 0x53
        std::byte{0x53},

        // qty: 100 = 0x64
        std::byte{0x64}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},

        // limix_px: 100000 = 0x186A0
        std::byte{0xA0}, std::byte{0x86},
        std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00},
    };

} // namespace fixtures