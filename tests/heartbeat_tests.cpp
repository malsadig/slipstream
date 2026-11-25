#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>

namespace {
    void testValidHeartbeatEncoding() {
        const Heartbeat heartbeat {
            .ts_ns = fixtures::timestamp,
        };

        std::array<std::byte, 12> output{};

        EncodeResult result = encode(heartbeat, output);

        assert(result.error == CodecError::NONE);
        assert(result.bytes_written == fixtures::heartbeatBytes.size());

        assert(output == fixtures::heartbeatBytes);
    }

    void testValidHeartbeatDecoding() {
        DecodeResult result = decodeOne(fixtures::heartbeatBytes);
        assert(result.status == DecodeStatus::COMPLETE);
        assert(result.error == CodecError::NONE);
        assert(result.bytes_consumed == kHeartbeatWireSize);
        
        assert(result.message.has_value());

        const Message& message = *result.message;

        const Heartbeat* heartbeat = std::get_if<Heartbeat>(&message);

        assert(heartbeat != nullptr);
        assert(heartbeat->ts_ns == fixtures::timestamp);
    }

    void testHeartbeatRoundTrips() {
        const Heartbeat basic {
            .ts_ns = fixtures::timestamp,
        };

        assertRoundTrip<kHeartbeatWireSize>(basic);

        Heartbeat zeroTimestamp = basic;
        zeroTimestamp.ts_ns = 0;

        assertRoundTrip<kHeartbeatWireSize>(zeroTimestamp);
    }
} // namespace

void runHeartbeatTests() {
    testValidHeartbeatEncoding();
    testValidHeartbeatDecoding();
    testHeartbeatRoundTrips();
}