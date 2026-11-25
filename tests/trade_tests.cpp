#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>

namespace {
    void testValidTradeEncoding() {
        const Trade trade {
            .symbol = "somesymbol",
            .ts_ns = fixtures::timestamp,
            .qty = 100,
            .px = 100000,
            .aggressor = Aggressor::BUY,
            .id = 42
        };

        std::array<std::byte, kTradeWireSize> output{};

        EncodeResult result = encode(trade, output);

        assert(result.error == CodecError::NONE);
        assert(result.bytes_written == fixtures::tradeBytes.size());

        assert(output == fixtures::tradeBytes);
    }

    void testValidTradeDecoding() {
        DecodeResult result = decodeOne(fixtures::tradeBytes);
        assert(result.status == DecodeStatus::COMPLETE);
        assert(result.error == CodecError::NONE);
        assert(result.bytes_consumed == kTradeWireSize);
        
        assert(result.message.has_value());

        const Message& message = *result.message;

        const Trade* trade = std::get_if<Trade>(&message);

        assert(trade != nullptr);
        assert(trade->ts_ns == fixtures::timestamp);
        assert(std::ranges::equal(trade->symbol, fixtures::expectedSymbol));
        assert(trade->px == 100000);
        assert(trade->qty == 100);
        assert(trade->aggressor == Aggressor::BUY);
        assert(trade->id == 42);
    }

    void testTradeRoundTrips() {
        const Trade basic{
            .symbol = "somesymb2",
            .ts_ns = fixtures::timestamp,
            .qty = 1825,
            .px = 762000,
            .aggressor = Aggressor::BUY,
            .id = 918291
        };

        assertRoundTrip<kTradeWireSize>(basic);
        
        Trade negativePrices = basic;
        negativePrices.px = -263000;

        assertRoundTrip<kTradeWireSize>(negativePrices);

        Trade fullSymbol = basic;
        std::copy_n("ABCDEFGHIJKL", kSymbolSize, fullSymbol.symbol);

        assertRoundTrip<kTradeWireSize>(fullSymbol);

        Trade sellSymbol = basic;
        sellSymbol.aggressor = Aggressor::SELL;

        assertRoundTrip<kTradeWireSize>(sellSymbol);

        Trade unknownSymbol = basic;
        unknownSymbol.aggressor = Aggressor::UNKNOWN;

        assertRoundTrip<kTradeWireSize>(unknownSymbol);
    }
} // namespace

void runTradeTests() {
    testValidTradeEncoding();
    testValidTradeDecoding();
    testTradeRoundTrips();
}