#include "slipstream/stream_decoder.hpp"
#include "support/codec_fixtures.hpp"
#include "test_suites.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace {
    struct StreamFixture {
        std::span<const std::byte> bytes;
        Message expected;
    };

    const std::array<StreamFixture, 4> streamFixtures {
        StreamFixture {
            fixtures::heartbeatBytes,
            Heartbeat {
                .ts_ns = fixtures::timestamp,
            }
        },
        StreamFixture {
            fixtures::quoteBytes,
            Quote{
                .symbol = "somesymbol",
                .ts_ns = fixtures::timestamp,
                .bid_qty = 0,
                .bid_px = 0,
                .ask_qty = 100,
                .ask_px = 100000,
            }
        },
        StreamFixture {
            fixtures::tradeBytes,
            Trade {
                .symbol = "somesymbol",
                .ts_ns = fixtures::timestamp,
                .qty = 100,
                .px = 100000,
                .aggressor = Aggressor::BUY,
                .id = 42,
            }
        },
        StreamFixture {
            fixtures::sessionControlBytes,
            SessionControl {
                .ts_ns = fixtures::timestamp,
                .state = SessionState::HALT,
            }
        },
    };

    void assertMessages(const StreamDecodeResult& result, const std::vector<Message>& expected, 
        CodecError expectedError = CodecError::NONE) {
        assert(result.error == expectedError);
        assert(result.messages == expected);
    }

    void testEverySplit() {
        for (const auto& fixture : streamFixtures) {
            for (auto split{0uz}; split <= fixture.bytes.size(); ++split) {
                StreamDecoder decoder;
                const auto first = decoder.feed(fixture.bytes.first(split));

                assert(first.error == CodecError::NONE);
                assert(first.messages.size() == (split == fixture.bytes.size() ? 1uz : 0uz));

                const auto second = decoder.feed(fixture.bytes.subspan(split));
                assert(second.error == CodecError::NONE);

                auto messages = first.messages;
                messages.insert(messages.end(), second.messages.begin(), second.messages.end());
                assert(messages == std::vector<Message>{fixture.expected});

                assertMessages(decoder.feed({}), {});
                assert(decoder.finish() == CodecError::NONE);
            }
        }
    }

    void testOneByteChunks() {
        for (const auto& fixture : streamFixtures) {
            StreamDecoder decoder;

            for (auto i{0uz}; i < fixture.bytes.size(); ++i) {
                const auto result = decoder.feed(fixture.bytes.subspan(i, 1));
                if (i + 1 == fixture.bytes.size()) {
                    assertMessages(result, {fixture.expected});
                } else {
                    assertMessages(result, {});
                }
            }

            assertMessages(decoder.feed({}), {});
            assert(decoder.finish() == CodecError::NONE);
        }
    }

    void testMultipleFrames() {
        std::vector<std::byte> bytes;
        std::vector<Message> expected;

        for (const auto& fixture : streamFixtures) {
            bytes.insert(bytes.end(), fixture.bytes.begin(), fixture.bytes.end());
            expected.push_back(fixture.expected);
        }

        StreamDecoder decoder;
        assertMessages(decoder.feed(bytes), expected);
        assertMessages(decoder.feed({}), {});
        assert(decoder.finish() == CodecError::NONE);
    }

    void testCompleteFrameAndPartialTail() {
        const auto& first = streamFixtures[0];
        const auto& next = streamFixtures[1];

        for (auto split{1uz}; split < next.bytes.size(); ++split) {
            std::vector<std::byte> bytes(first.bytes.begin(), first.bytes.end());
            const auto prefix = next.bytes.first(split);
            bytes.insert(bytes.end(), prefix.begin(), prefix.end());

            StreamDecoder decoder;
            assertMessages(decoder.feed(bytes), {first.expected});
            assertMessages(decoder.feed({}), {});
            assertMessages(decoder.feed(next.bytes.subspan(split)), {next.expected});
            assertMessages(decoder.feed({}), {});
            assert(decoder.finish() == CodecError::NONE);
        }
    }

    void assertStreamError(std::span<const std::byte> bytes, CodecError expectedError) {
        StreamDecoder decoder;
        assertMessages(decoder.feed(bytes), {}, expectedError);
        assertMessages(decoder.feed({}), {}, expectedError);
        assertMessages(decoder.feed(fixtures::heartbeatBytes), {}, expectedError);
        assert(decoder.finish() == expectedError);
    }

    void testMalformedFrames() {
        auto invalidVersion = fixtures::heartbeatBytes;
        invalidVersion[3] = std::byte{0xFF};
        assertStreamError(invalidVersion, CodecError::UNSUPPORTED_VERSION);

        auto invalidType = fixtures::heartbeatBytes;
        invalidType[2] = std::byte{0xFF};
        assertStreamError(invalidType, CodecError::UNKNOWN_MESSAGE_TYPE);

        auto invalidLength = fixtures::heartbeatBytes;
        invalidLength[0] = std::byte{0x07};
        assertStreamError(invalidLength, CodecError::INVALID_BODY_LENGTH);

        auto invalidAggressor = fixtures::tradeBytes;
        invalidAggressor[36] = std::byte{0x58}; // 'X' in the aggressor field.
        assertStreamError(invalidAggressor, CodecError::INVALID_FIELD);

        auto invalidState = fixtures::sessionControlBytes;
        invalidState[12] = std::byte{0xFF};
        assertStreamError(invalidState, CodecError::INVALID_FIELD);
    }

    void testValidFrameBeforeError() {
        const auto& valid = streamFixtures[0];
        auto invalid = fixtures::heartbeatBytes;
        invalid[3] = std::byte{0xFF};

        std::vector<std::byte> bytes(valid.bytes.begin(), valid.bytes.end());
        bytes.insert(bytes.end(), invalid.begin(), invalid.end());
        bytes.insert(bytes.end(), valid.bytes.begin(), valid.bytes.end());

        StreamDecoder decoder;
        assertMessages(decoder.feed(bytes), {valid.expected},
                    CodecError::UNSUPPORTED_VERSION);
        assertMessages(decoder.feed(valid.bytes), {},
                    CodecError::UNSUPPORTED_VERSION);
        assert(decoder.finish() == CodecError::UNSUPPORTED_VERSION);
    }

    void testEndOfInput() {
        StreamDecoder empty;
        assert(empty.finish() == CodecError::NONE);

        for (const auto& fixture : streamFixtures) {
            for (auto size{1uz}; size < fixture.bytes.size(); ++size) {
                StreamDecoder decoder;
                assertMessages(decoder.feed(fixture.bytes.first(size)), {});

                assert(decoder.finish() == CodecError::TRUNCATED_INPUT);
                assert(decoder.finish() == CodecError::TRUNCATED_INPUT);
                assertMessages(decoder.feed(fixture.bytes.subspan(size)), {}, CodecError::TRUNCATED_INPUT);
            }
        }

        const auto& first = streamFixtures[0];
        const auto& next = streamFixtures[1];
        std::vector<std::byte> bytes(first.bytes.begin(), first.bytes.end());
        const auto prefix = next.bytes.first(5);
        bytes.insert(bytes.end(), prefix.begin(), prefix.end());

        StreamDecoder decoder;
        assertMessages(decoder.feed(bytes), {first.expected});
        assert(decoder.finish() == CodecError::TRUNCATED_INPUT);
    }

} // namespace

void runStreamTests() {
    testEverySplit();
    testOneByteChunks();
    testMultipleFrames();
    testCompleteFrameAndPartialTail();
    testMalformedFrames();
    testValidFrameBeforeError();
    testEndOfInput();
}