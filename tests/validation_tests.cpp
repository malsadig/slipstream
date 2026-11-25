#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <algorithm>

namespace {
    void assertIncompletePrefixes(std::span<const std::byte> frame) {
        for (auto size{0uz}; size < frame.size(); ++size) {
            const auto result = decodeOne(frame.first(size));

            assert(result.status == DecodeStatus::NEED_MORE);
            assert(result.error == CodecError::NONE);
            assert(result.bytes_consumed == 0);
            assert(!result.message.has_value());
        }
    }

    void testIncompleteInput() {
        assertIncompletePrefixes(fixtures::heartbeatBytes);
        assertIncompletePrefixes(fixtures::quoteBytes);
        assertIncompletePrefixes(fixtures::tradeBytes);
        assertIncompletePrefixes(fixtures::sessionControlBytes);
    }

    void testMalformedHeaders() {
        auto invalidVersion = fixtures::heartbeatBytes;
        invalidVersion[3] = std::byte{0xFF};
        assertDecodeError(
            std::span<const std::byte>{invalidVersion}.first<kHeaderSize>(),
            CodecError::UNSUPPORTED_VERSION
        );

        auto unknownMessage = fixtures::heartbeatBytes;
        unknownMessage[2] = std::byte{0xFF};
        assertDecodeError(
            std::span<const std::byte>{unknownMessage}.first<kHeaderSize>(),
            CodecError::UNKNOWN_MESSAGE_TYPE
        );

        auto invalidMessage = fixtures::heartbeatBytes;
        invalidMessage[2] = std::byte{0x00};
        assertDecodeError(
            std::span<const std::byte>{invalidMessage}.first<kHeaderSize>(),
            CodecError::UNKNOWN_MESSAGE_TYPE
        );

        auto invalidBodyLength = fixtures::heartbeatBytes;
        invalidBodyLength[0] = std::byte{0x09};
        assertDecodeError(
            std::span<const std::byte>{invalidBodyLength}.first<kHeaderSize>(),
            CodecError::INVALID_BODY_LENGTH
        );
    }

    void testShortOutputBuffers() {
        const Trade trade {
            .symbol = "somesymbol",
            .ts_ns = fixtures::timestamp,
            .qty = 100,
            .px = 100000,
            .aggressor = Aggressor::BUY,
            .id = 42
        };

        const SessionControl sessionControl {
            .ts_ns = fixtures::timestamp,
            .state = SessionState::HALT
        };

        const Quote quote {
            .symbol = "somenewsymb",
            .ts_ns = fixtures::timestamp,
            .bid_qty = 25,
            .bid_px = 263000,
            .ask_qty = 100,
            .ask_px = 264000
        };

        const Heartbeat heartbeat {
            .ts_ns = fixtures::timestamp,
        };

        assertShortOutputBuffers<kHeartbeatWireSize>(heartbeat);
        assertShortOutputBuffers<kQuoteWireSize>(quote);
        assertShortOutputBuffers<kTradeWireSize>(trade);
        assertShortOutputBuffers<kSessionControlWireSize>(sessionControl);
    }

    void testInvalidFields() {
        auto invalidAggressor = fixtures::tradeBytes;
        invalidAggressor[36] = std::byte{0x58}; // 'X'

        assertDecodeError(invalidAggressor, CodecError::INVALID_FIELD);

        auto invalidState = fixtures::sessionControlBytes;
        invalidState[12] = std::byte{0xFF};

        assertDecodeError(invalidState, CodecError::INVALID_FIELD);
    }

    void testInvalidEncoding() {
        Trade invalidTrade {
            .symbol = "somesymbol",
            .ts_ns = fixtures::timestamp,
            .qty = 100,
            .px = 100000,
            .aggressor = Aggressor::BUY,
            .id = 42
        };

        SessionControl invalidSessionControl {
            .ts_ns = fixtures::timestamp,
            .state = SessionState::HALT
        };


        invalidTrade.aggressor = static_cast<Aggressor>('X');
        invalidSessionControl.state = static_cast<SessionState>(0xFF);

        assertEncodeError<kTradeWireSize>(
            invalidTrade, CodecError::INVALID_FIELD
        );

        assertEncodeError<kSessionControlWireSize>(
            invalidSessionControl, CodecError::INVALID_FIELD
        );
    }
} // namespace

void runValidationTests() {
    testIncompleteInput();
    testMalformedHeaders();
    testShortOutputBuffers();
    testInvalidFields();
    testInvalidEncoding();
}