#pragma once

#include "slipstream/messages.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace slipstream::detail {
    inline void writeU16LittleEndian(std::uint16_t value, std::span<std::byte, 2> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
    }

    inline void writeU32LittleEndian(std::uint32_t value, std::span<std::byte, 4> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
        output[2] = static_cast<std::byte>((value >> 16) & 0xFFu);
        output[3] = static_cast<std::byte>((value >> 24) & 0xFFu);
    }

    inline void writeU64LittleEndian(std::uint64_t value, std::span<std::byte, 8> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
        output[2] = static_cast<std::byte>((value >> 16) & 0xFFu);
        output[3] = static_cast<std::byte>((value >> 24) & 0xFFu);
        output[4] = static_cast<std::byte>((value >> 32) & 0xFFu);
        output[5] = static_cast<std::byte>((value >> 40) & 0xFFu);
        output[6] = static_cast<std::byte>((value >> 48) & 0xFFu);
        output[7] = static_cast<std::byte>((value >> 56) & 0xFFu);
    }

    inline void writeI64LittleEndian(std::int64_t value, std::span<std::byte, 8> output) {
        writeU64LittleEndian(static_cast<std::uint64_t>(value), output);
    }

    inline void writeHeader(const Header& header, std::span<std::byte, 4> output) {
        writeU16LittleEndian(header.body_len, output.first<2>());
        output[2] = static_cast<std::byte>(header.msg_type);
        output[3] = static_cast<std::byte>(header.version);
    }

    inline void writeSymbol(std::span<const char, 12> symbol, std::span<std::byte, 12> output) {
        for (auto i{0uz}; i < symbol.size(); ++i) {
            output[i] = static_cast<std::byte>(symbol[i]);
        }
    }

    class ByteWriter {
    public:
        explicit ByteWriter(std::span<std::byte> output, MessageType messageType)
            : remaining(output), initialSize(output.size()), messageType(messageType) {
                std::optional<uint16_t> tryWireSize = wireSize(messageType);
                if (!tryWireSize) {
                    throw std::logic_error("Bad message type passed to byte writer.");
                }
                mWireSize = *tryWireSize;
            }

        void header() {
            writeHeader({
                .body_len = static_cast<std::uint16_t>(mWireSize - kHeaderSize),
                .msg_type = messageType,
            }, take<kHeaderSize>());
        }

        void symbol(std::span<const char, kSymbolSize> value) {
            writeSymbol(value, take<kSymbolSize>());
        }

        void u32(std::uint32_t value) {
            writeU32LittleEndian(value, take<sizeof(value)>());
        }

        void u64(std::uint64_t value) {
            writeU64LittleEndian(value, take<sizeof(value)>());
        }

        void u8(std::uint8_t value) {
            take<1>()[0] = static_cast<std::byte>(value);
        }

        void character(char value) {
            take<1>()[0] = static_cast<std::byte>(value);
        }

        void i64(std::int64_t value) {
            writeI64LittleEndian(value, take<sizeof(value)>());
        }

        std::size_t bytesWritten() const {
            return initialSize - remaining.size();
        }

    private:
        template<std::size_t N>
        std::span<std::byte, N> take() {
            assert(remaining.size() >= N);

            auto field = remaining.first<N>();
            remaining = remaining.subspan<N>();
            return field;
        }

        std::span<std::byte> remaining{};
        std::size_t initialSize{};

        MessageType messageType{};
        uint16_t mWireSize{};
    };
}