#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>

void testValidSessionControlEncoding() {
    const SessionControl sessionControl {
        .ts_ns = fixtures::timestamp,
        .state = SessionState::HALT
    };

    std::array<std::byte, kSessionControlWireSize> output{};

    EncodeResult result = encode(sessionControl, output);

    assert(result.error == CodecError::NONE);
    assert(result.bytes_written == fixtures::sessionControlBytes.size());

    assert(output == fixtures::sessionControlBytes);
}

void testValidSessionControlDecoding() {
    DecodeResult result = decodeOne(fixtures::sessionControlBytes);
    assert(result.status == DecodeStatus::COMPLETE);
    assert(result.error == CodecError::NONE);
    assert(result.bytes_consumed == kSessionControlWireSize);
    
    assert(result.message.has_value());

    const Message& message = *result.message;

    const SessionControl* sessionControl = std::get_if<SessionControl>(&message);

    assert(sessionControl != nullptr);
    assert(sessionControl->ts_ns == fixtures::timestamp);
    assert(sessionControl->state == SessionState::HALT);
}

void testSessionControlRoundTrips() {
    const SessionControl basic {
        .ts_ns = fixtures::timestamp,
        .state = SessionState::HALT
    };

    assertRoundTrip<kSessionControlWireSize>(basic);
    
    SessionControl close = basic;
    close.state = SessionState::CLOSE;

    assertRoundTrip<kSessionControlWireSize>(close);

    SessionControl open = basic;
    open.state = SessionState::OPEN;

    assertRoundTrip<kSessionControlWireSize>(open);
}

void runSessionControlTests() {
    testValidSessionControlEncoding();
    testValidSessionControlDecoding();
    testSessionControlRoundTrips();
}