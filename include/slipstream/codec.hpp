#pragma once

#include "messages.hpp"

#include <variant>
#include <cstddef>
#include <optional>
#include <span>



enum class CodecError {
    NONE,
    BUFFER_TOO_SMALL,
    INVALID_FIELD,
    UNSUPPORTED_VERSION,
    UNKNOWN_MESSAGE_TYPE,
    INVALID_BODY_LENGTH,
};

struct EncodeResult {
    CodecError error{CodecError::NONE};
    std::size_t bytes_written{};
};

using Message = std::variant<Heartbeat, Quote, Trade, SessionControl>;

enum class DecodeStatus {
    COMPLETE,
    NEED_MORE,
    ERROR,
};

struct DecodeResult {
    DecodeStatus status{DecodeStatus::NEED_MORE};
    CodecError error{CodecError::NONE};
    std::size_t bytes_consumed{};
    std::optional<Message> message{};
};

// Each overload writes a complete frame: header + body.
[[nodiscard]] EncodeResult encode(const Heartbeat& message, std::span<std::byte> output);

[[nodiscard]] EncodeResult encode(const Quote& message, std::span<std::byte> output);

[[nodiscard]] EncodeResult encode(const Trade& message, std::span<std::byte> output);

[[nodiscard]] EncodeResult encode(const SessionControl& message, std::span<std::byte> output);

// Attempts to decode the first frame in input.
[[nodiscard]] DecodeResult decodeOne(std::span<const std::byte> input);