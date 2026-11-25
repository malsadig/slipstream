#pragma once

#include "slipstream/messages.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace slipstream::detail {
    inline std::uint16_t readU16LittleEndian(std::span<const std::byte, 2> input) {
        const auto first  = std::to_integer<std::uint16_t>(input[0]);
        const auto second = std::to_integer<std::uint16_t>(input[1]);

        return static_cast<std::uint16_t>(first | (second << 8));
    }

    inline std::uint32_t readU32LittleEndian(std::span<const std::byte, 4> input) {
        const auto first  = std::to_integer<std::uint32_t>(input[0]);
        const auto second = std::to_integer<std::uint32_t>(input[1]);
        const auto third = std::to_integer<std::uint32_t>(input[2]);
        const auto fourth = std::to_integer<std::uint32_t>(input[3]);

        return static_cast<std::uint32_t>(first | (second << 8) | (third << 16) | (fourth << 24));
    }

    inline std::uint64_t readU64LittleEndian(std::span<const std::byte, 8> input) {
        const auto first  = std::to_integer<std::uint64_t>(input[0]);
        const auto second = std::to_integer<std::uint64_t>(input[1]);
        const auto third = std::to_integer<std::uint64_t>(input[2]);
        const auto fourth = std::to_integer<std::uint64_t>(input[3]);
        const auto fifth = std::to_integer<std::uint64_t>(input[4]);
        const auto sixth = std::to_integer<std::uint64_t>(input[5]);
        const auto seventh = std::to_integer<std::uint64_t>(input[6]);
        const auto eighth = std::to_integer<std::uint64_t>(input[7]);

        return static_cast<std::uint64_t>(
            first | (second << 8) | (third << 16) | (fourth << 24)
            | (fifth << 32)| (sixth << 40) | (seventh << 48) | (eighth << 56)
        );
    }

    inline std::int64_t readI64LittleEndian(std::span<const std::byte, 8> input) {
        return std::bit_cast<std::int64_t>(
            readU64LittleEndian(input)
        );
    }

    inline Header readHeader(std::span<const std::byte, kHeaderSize> input) {
        return Header{
            .body_len = readU16LittleEndian(input.first<2>()),
            .msg_type = static_cast<MessageType>(input[2]),
            .version  = std::to_integer<std::uint8_t>(input[3])
        };
    }

    inline void readSymbol(std::span<const std::byte, kSymbolSize> input, std::span<char, kSymbolSize> output) {
        for (std::size_t i = 0; i < kSymbolSize; ++i) {
            output[i] = std::to_integer<char>(input[i]);
        }
    }

    class ByteReader {
    public:
        explicit ByteReader(std::span<const std::byte> input)
            : remaining(input) {}

        void symbol(std::span<char, kSymbolSize> output) {
            readSymbol(take<kSymbolSize>(), output);
        }

        std::uint32_t u32() {
            return readU32LittleEndian(take<4>());
        }

        std::uint64_t u64() {
            return readU64LittleEndian(take<8>());
        }

        std::int64_t i64() {
            return readI64LittleEndian(take<8>());
        }

        std::uint8_t u8() {
            return std::to_integer<std::uint8_t>(take<1>()[0]);
        }

        char character() {
            return std::to_integer<char>(take<1>()[0]);
        }

    private:
        template<std::size_t N>
        std::span<const std::byte, N> take() {
            assert(remaining.size() >= N);

            auto field = remaining.first<N>();
            remaining = remaining.subspan<N>();
            return field;
        }

        std::span<const std::byte> remaining;
    };
}