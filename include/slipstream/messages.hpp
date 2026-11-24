//WIRE RULES:
// 1. all integers are little-endian
// 2. fields have no padding
// 3. prices are signed 64-bit integers multiplied by 10,000
// 4. symbols occupy exactly 12 bytes
//
// These structs hold values; the codec produces the wire format.

#pragma once

#include <cstdint>

#pragma pack(push, 1)

struct Heartbeat {
    uint64_t ts_ns{};
};

struct Header {
    uint16_t body_len{}; // Length of body only; header excluded
    uint8_t msg_type{}; // 1 = Quote, 2 = Trade, 3 = Heartbeat, 4 = SessionControl
    uint8_t version{1};
};

struct Quote {
    char symbol[12]{}; // 12 byte, ASCII, null-padded, not null-terminated if full
    uint64_t ts_ns{}; // Nanoseconds since Unix epoch
    uint32_t bid_qty{};
    int64_t bid_px{}; // Fixed point ×10,000
    uint32_t ask_qty{};
    int64_t ask_px{}; // Fixed point ×10,000
};

struct Trade {
    char symbol[12]{};
    uint64_t ts_ns{};
    uint32_t qty{};
    int64_t px{};
    char aggressor{'?'}; // 'B', 'S', or '?' if unknown
    int64_t id{}; // Trade identifier
};

struct SessionControl {
    uint64_t ts_ns{};
    uint8_t state{}; // 0=OPEN, 1=HALT, 2=CLOSE
};

#pragma pack(pop)

static_assert(sizeof(Heartbeat) == 8);
static_assert(sizeof(Header) == 4);
static_assert(sizeof(Quote) == 44);
static_assert(sizeof(Trade) == 41);
static_assert(sizeof(SessionControl) == 9);