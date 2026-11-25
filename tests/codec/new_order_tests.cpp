#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>
#include <limits>

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

    void testNewOrderDecodingFailures() {
        auto invalidVersion = fixtures::newOrderBytes;
        invalidVersion[3] = std::byte{0xFF};
        assertDecodeError(
            std::span<const std::byte>{invalidVersion}.first<kHeaderSize>(),
            CodecError::UNSUPPORTED_VERSION
        );

        auto invalidBodyLength = fixtures::newOrderBytes;
        invalidBodyLength[0] = std::byte{0x31};
        assertDecodeError(
            std::span<const std::byte>{invalidBodyLength}.first<kHeaderSize>(),
            CodecError::INVALID_BODY_LENGTH
        );

        invalidBodyLength[0] = std::byte{0x33};
        assertDecodeError(
            std::span<const std::byte>{invalidBodyLength}.first<kHeaderSize>(),
            CodecError::INVALID_BODY_LENGTH
        );

        auto invalidStatus = fixtures::newOrderBytes;
        invalidStatus[24] = std::byte{0x58};
        assertDecodeError(
            std::span<const std::byte>{invalidStatus},
            CodecError::INVALID_FIELD
        );

        auto invalidSide = fixtures::newOrderBytes;
        invalidSide[41] = std::byte{0x3F};
        assertDecodeError(
            std::span<const std::byte>{invalidSide},
            CodecError::INVALID_FIELD
        );
    }

    void testNewOrderRoundTrips() {
        NewOrder acceptedSell = expected;
        assertRoundTrip<kNewOrderWireSize>(acceptedSell);

        NewOrder acceptedBuy = expected;
        acceptedBuy.side = OrderSide::BUY;
        assertRoundTrip<kNewOrderWireSize>(acceptedBuy);

        NewOrder rejectedSell = expected;
        rejectedSell.status = OrderStatus::REJECTED;
        assertRoundTrip<kNewOrderWireSize>(rejectedSell);

        NewOrder rejectedBuy = rejectedSell;
        rejectedBuy.side = OrderSide::BUY;
        assertRoundTrip<kNewOrderWireSize>(rejectedBuy);

        NewOrder fullSymbol = expected;
        std::copy_n("ABCDEFGHIJKL", kSymbolSize, fullSymbol.symbol);
        assertRoundTrip<kNewOrderWireSize>(fullSymbol);

        NewOrder negativeInts = expected;
        negativeInts.trade_id = -42;
        negativeInts.limit_px = -263000;
        assertRoundTrip<kNewOrderWireSize>(negativeInts);

        NewOrder zerod = expected;
        zerod.client_order_id = 0;
        zerod.trade_id = 0;
        zerod.ts_ns = 0;
        zerod.qty = 0;
        zerod.limit_px = 0;
        assertRoundTrip<kNewOrderWireSize>(zerod);

        NewOrder minimumSigned = expected;
        minimumSigned.trade_id = std::numeric_limits<std::int64_t>::min();
        minimumSigned.limit_px = std::numeric_limits<std::int64_t>::min();
        assertRoundTrip<kNewOrderWireSize>(minimumSigned);

        NewOrder maximumSigned = expected;
        maximumSigned.trade_id = std::numeric_limits<std::int64_t>::max();
        maximumSigned.limit_px = std::numeric_limits<std::int64_t>::max();
        assertRoundTrip<kNewOrderWireSize>(maximumSigned);

        NewOrder maximumUnsigned = expected;
        maximumUnsigned.client_order_id = std::numeric_limits<std::uint64_t>::max();
        maximumUnsigned.ts_ns = std::numeric_limits<std::uint64_t>::max();
        maximumUnsigned.qty = std::numeric_limits<std::uint32_t>::max();
        assertRoundTrip<kNewOrderWireSize>(maximumUnsigned);
    }
} // namespace

void runNewOrderTests() {
    testValidNewOrderEncoding();
    testNewOrderEncodingFailures();
    testValidNewOrderDecoding();
    testNewOrderDecodingFailures();
    testNewOrderRoundTrips();
}
