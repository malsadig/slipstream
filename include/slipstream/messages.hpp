//WIRE RULES:
// 1. all integers are little-endian
// 2. fields have no padding
// 3. prices are signed 64-bit integers multiplied by 10,000
// 4. symbols occupy exactly 12 bytes
//
// These structs hold values; the codec produces the wire format.

#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>

#pragma pack(push, 1)

inline constexpr std::uint8_t kProtocolVersion{1};

struct Heartbeat {
    uint64_t ts_ns{};
};

enum class MessageType : std::uint8_t {
    INVALID = 0,  // Local unset value; not a valid wire message.
    QUOTE = 1,
    TRADE = 2,
    HEARTBEAT = 3,
    SESSION_CONTROL = 4
};

struct Header {
    uint16_t body_len{}; // Length of body only; header excluded
    MessageType msg_type{MessageType::INVALID};
    uint8_t version{kProtocolVersion};
};

struct Quote {
    char symbol[12]{}; // 12 byte, ASCII, null-padded, not null-terminated if full
    uint64_t ts_ns{}; // Nanoseconds since Unix epoch
    uint32_t bid_qty{};
    int64_t bid_px{}; // Fixed point ×10,000
    uint32_t ask_qty{};
    int64_t ask_px{}; // Fixed point ×10,000
};

enum class Aggressor : char {
    BUY = 'B',
    SELL = 'S',
    UNKNOWN = '?'
};

struct Trade {
    char symbol[12]{};
    uint64_t ts_ns{};
    uint32_t qty{};
    int64_t px{};
    Aggressor aggressor{Aggressor::UNKNOWN}; // 'B', 'S', or '?' if unknown
    int64_t id{}; // Trade identifier
};

enum class SessionState : std::uint8_t {
    OPEN = 0,
    HALT = 1,
    CLOSE = 2
};

struct SessionControl {
    uint64_t ts_ns{};
    SessionState state{SessionState::OPEN};
};

#pragma pack(pop)

static_assert(sizeof(Heartbeat) == 8);
static_assert(sizeof(Header) == 4);
static_assert(sizeof(Quote) == 44);
static_assert(sizeof(Trade) == 41);
static_assert(sizeof(SessionControl) == 9);

inline constexpr std::uint8_t kSymbolSize{12};
inline constexpr std::uint8_t kHeaderSize{sizeof(Header)}; // 4

inline constexpr std::uint8_t kQuoteSize{sizeof(Quote)}; // 44
inline constexpr std::uint8_t kHeartbeatSize{sizeof(Heartbeat)}; // 8
inline constexpr std::uint8_t kTradeSize{sizeof(Trade)}; // 41
inline constexpr std::uint8_t kSessionControlSize{sizeof(SessionControl)}; // 9

inline constexpr std::uint8_t kHeartbeatWireSize{kHeaderSize + kHeartbeatSize}; // 12
inline constexpr std::uint8_t kQuoteWireSize{kHeaderSize + kQuoteSize}; // 48
inline constexpr std::uint8_t kTradeWireSize{kHeaderSize + kTradeSize}; // 45
inline constexpr std::uint8_t kSessionControlWireSize{kHeaderSize + kSessionControlSize}; // 13

// each new message type will require a constexpr entry above, and a switch case here
constexpr std::optional<std::size_t> bodySize(MessageType messageType) noexcept {
    switch (messageType) {
        case MessageType::QUOTE:
            return kQuoteSize;

        case MessageType::TRADE:
            return kTradeSize;

        case MessageType::HEARTBEAT:
            return kHeartbeatSize;

        case MessageType::SESSION_CONTROL:
            return kSessionControlSize;

        case MessageType::INVALID:
            return std::nullopt;
    }

    return std::nullopt;
}

constexpr std::optional<std::size_t> wireSize(MessageType messageType) noexcept {
    return bodySize(messageType).transform(
        [](std::size_t bodySize) {
            return kHeaderSize + bodySize;
        }
    );
}

static_assert(wireSize(MessageType::QUOTE).value() == 48);
static_assert(!wireSize(MessageType::INVALID));