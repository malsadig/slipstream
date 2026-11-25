#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>

namespace {
    const NewOrder expected {
        .client_order_id = 0x1112131415161718ULL,
        .symbol = "somesymbol",
        .status = OrderStatus::ACCEPTED,
        .ts_ns = fixtures::timestamp,
        .trade_id = 42,
        .side = OrderSide::SELL,
        .qty = 100,
        .limit_px = 100000,
    };

    void testValidNewOrderEncoding() {

        std::array<std::byte, kNewOrderWireSize> output{};

        EncodeResult result = encode(expected, output);

        assert(result.error == CodecError::NONE);
        assert(result.bytes_written == fixtures::newOrderBytes.size());

        assert(output == fixtures::newOrderBytes);
    }
    
    void testNewOrderEncodingFailures() {
        assertShortOutputBuffers<kNewOrderWireSize>(expected);

        NewOrder invalidStatus = expected;
        invalidStatus.status = static_cast<OrderStatus>('X');

        assertEncodeError<kNewOrderWireSize>(invalidStatus, CodecError::INVALID_FIELD);

        NewOrder invalidSide = expected;
        invalidSide.side = static_cast<OrderSide>('?');

        assertEncodeError<kNewOrderWireSize>(invalidSide, CodecError::INVALID_FIELD);

        assertShortOutputBuffers<kNewOrderWireSize>(invalidStatus);
        assertShortOutputBuffers<kNewOrderWireSize>(invalidSide);
    }

    void testValidNewOrderDecoding() {
        DecodeResult result = decodeOne(fixtures::newOrderBytes);
        assert(result.status == DecodeStatus::COMPLETE);
        assert(result.error == CodecError::NONE);
        assert(result.bytes_consumed == kNewOrderWireSize);
        
        assert(result.message.has_value());

        const Message& message = *result.message;

        const NewOrder* newOrder = std::get_if<NewOrder>(&message);

        assert(newOrder != nullptr);
        assert(*newOrder == expected);
    }
} // namespace

void runNewOrderTests() {
    testValidNewOrderEncoding();
    testNewOrderEncodingFailures();
    testValidNewOrderDecoding();
}