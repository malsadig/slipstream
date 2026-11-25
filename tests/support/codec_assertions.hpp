#pragma once

#include "slipstream/codec.hpp"

#include <cassert>
#include <array>
#include <variant>

template<std::size_t WireSize, typename T>
void assertRoundTrip(const T& original) {
    std::array<std::byte, WireSize> output{};

    const auto encoded = encode(original, output);

    assert(encoded.error == CodecError::NONE);
    assert(encoded.bytes_written == WireSize);

    const auto decoded = decodeOne(
        std::span<const std::byte>{output}.first(encoded.bytes_written)
    );

    assert(decoded.status == DecodeStatus::COMPLETE);
    assert(decoded.error == CodecError::NONE);
    assert(decoded.bytes_consumed == encoded.bytes_written);
    assert(decoded.message.has_value());

    const T* message = std::get_if<T>(&*decoded.message);

    assert(message != nullptr);
    assert(*message == original);
}

template<std::size_t WireSize, typename T>
void assertShortOutputBuffers(const T& message) {
    for (std::size_t size = 0; size < WireSize; ++size) {
        std::array<std::byte, WireSize> output{};
        output.fill(std::byte{0xA5});

        const auto before = output;

        const auto result = encode(
            message,
            std::span<std::byte>{output}.first(size)
        );

        assert(result.error == CodecError::BUFFER_TOO_SMALL);
        assert(result.bytes_written == 0);
        assert(output == before);
    }
}

inline void assertDecodeError(std::span<const std::byte> input, CodecError expectedError) {
    const auto result = decodeOne(input);

    assert(result.status == DecodeStatus::ERROR);
    assert(result.error == expectedError);
    assert(result.bytes_consumed == 0);
    assert(!result.message.has_value());
}

template<std::size_t WireSize, typename T>
void assertEncodeError(const T& message, CodecError expectedError) {
    std::array<std::byte, WireSize> output{};
    output.fill(std::byte{0xA5});

    const auto before = output;
    const auto result = encode(message, output);

    assert(result.error == expectedError);
    assert(result.bytes_written == 0);
    assert(output == before);
}