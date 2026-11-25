#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"

#include <cassert>
#include <array>

void testHeartbeatEncoding() {
    constexpr std::uint64_t timestamp = 0x0102030405060708ULL;
    Heartbeat heartbeat {
        .ts_ns = timestamp,
    };

    std::array<std::byte, 12> output{};

    constexpr std::array<std::byte, 12> expected {
        // Header: body length = 8, type = Heartbeat, version = 1
        std::byte{0x08}, std::byte{0x00},
        std::byte{0x03}, std::byte{0x01},

        // Timestamp: 0x0102030405060708, little-endian
        std::byte{0x08}, std::byte{0x07},
        std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03},
        std::byte{0x02}, std::byte{0x01}
    };

    EncodeResult result = encode(heartbeat, output);

    assert(result.error == CodecError::NONE);
    assert(result.bytes_written == expected.size());

    assert(output == expected);
}

int main() {
    testHeartbeatEncoding();
    //testHeartbeatDecoding();
    return 0;
}
