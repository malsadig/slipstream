#include "slipstream/codec.hpp"
#include <stdexcept>
#include <utility>
#include <cassert>
#include <bit>

namespace {
    // WRITE HELPERS
    void writeU16LittleEndian(std::uint16_t value, std::span<std::byte, 2> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
    }

    void writeU32LittleEndian(std::uint32_t value, std::span<std::byte, 4> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
        output[2] = static_cast<std::byte>((value >> 16) & 0xFFu);
        output[3] = static_cast<std::byte>((value >> 24) & 0xFFu);
    }

    void writeU64LittleEndian(std::uint64_t value, std::span<std::byte, 8> output) {
        output[0] = static_cast<std::byte>(value & 0xFFu);
        output[1] = static_cast<std::byte>((value >> 8) & 0xFFu);
        output[2] = static_cast<std::byte>((value >> 16) & 0xFFu);
        output[3] = static_cast<std::byte>((value >> 24) & 0xFFu);
        output[4] = static_cast<std::byte>((value >> 32) & 0xFFu);
        output[5] = static_cast<std::byte>((value >> 40) & 0xFFu);
        output[6] = static_cast<std::byte>((value >> 48) & 0xFFu);
        output[7] = static_cast<std::byte>((value >> 56) & 0xFFu);
    }

    void writeI64LittleEndian(std::int64_t value, std::span<std::byte, 8> output) {
        writeU64LittleEndian(static_cast<std::uint64_t>(value), output);
    }

    void writeHeader(const Header& header, std::span<std::byte, 4> output) {
        writeU16LittleEndian(header.body_len, output.first<2>());
        output[2] = static_cast<std::byte>(header.msg_type);
        output[3] = static_cast<std::byte>(header.version);
    }

    void writeSymbol(std::span<const char, 12> symbol, std::span<std::byte, 12> output) {
        for (auto i{0uz}; i < symbol.size(); ++i) {
            output[i] = static_cast<std::byte>(symbol[i]);
        }
    }

    // LITTLE ENDIAN READ HELPERS
    std::uint16_t readU16LittleEndian(std::span<const std::byte, 2> input) {
        const auto first  = std::to_integer<std::uint16_t>(input[0]);
        const auto second = std::to_integer<std::uint16_t>(input[1]);

        return static_cast<std::uint16_t>(first | (second << 8));
    }

    std::uint32_t readU32LittleEndian(std::span<const std::byte, 4> input) {
        const auto first  = std::to_integer<std::uint32_t>(input[0]);
        const auto second = std::to_integer<std::uint32_t>(input[1]);
        const auto third = std::to_integer<std::uint32_t>(input[2]);
        const auto fourth = std::to_integer<std::uint32_t>(input[3]);

        return static_cast<std::uint32_t>(first | (second << 8) | (third << 16) | (fourth << 24));
    }

    std::uint64_t readU64LittleEndian(std::span<const std::byte, 8> input) {
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

    std::int64_t readI64LittleEndian(std::span<const std::byte, 8> input) {
        return std::bit_cast<std::int64_t>(
            readU64LittleEndian(input)
        );
    }

    Header readHeader(std::span<const std::byte, kHeaderSize> input) {
        return Header{
            .body_len = readU16LittleEndian(input.first<2>()),
            .msg_type = static_cast<MessageType>(input[2]),
            .version  = std::to_integer<std::uint8_t>(input[3])
        };
    }

    void readSymbol(std::span<const std::byte, kSymbolSize> input, std::span<char, kSymbolSize> output) {
        for (std::size_t i = 0; i < kSymbolSize; ++i) {
            output[i] = std::to_integer<char>(input[i]);
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

EncodeResult encode(const Heartbeat& message, std::span<std::byte> output) {
    if (output.size() < kHeartbeatWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    ByteWriter writer{output, MessageType::HEARTBEAT};

    writer.header();
    writer.u64(message.ts_ns);

    return {CodecError::NONE, writer.bytesWritten()};
}

EncodeResult encode(const Quote& message, std::span<std::byte> output) {
    if (output.size() < kQuoteWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    ByteWriter writer{output, MessageType::QUOTE};

    writer.header();
    writer.symbol(message.symbol);
    writer.u64(message.ts_ns);
    writer.u32(message.bid_qty);
    writer.i64(message.bid_px);
    writer.u32(message.ask_qty);
    writer.i64(message.ask_px);

    return {CodecError::NONE, writer.bytesWritten()};
}

EncodeResult encode(const Trade& message, std::span<std::byte> output) {
    if (output.size() < kTradeWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    ByteWriter writer{output, MessageType::TRADE};

    writer.header();
    writer.symbol(message.symbol);
    writer.u64(message.ts_ns);
    writer.u32(message.qty);
    writer.i64(message.px);
    writer.character(std::to_underlying(message.aggressor));
    writer.i64(message.id);

    return {CodecError::NONE, writer.bytesWritten()};
}

EncodeResult encode(const SessionControl& message, std::span<std::byte> output) {
    if (output.size() < kSessionControlWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    ByteWriter writer{output, MessageType::SESSION_CONTROL};

    writer.header();
    writer.u64(message.ts_ns);
    writer.u8(std::to_underlying(message.state));

    return {CodecError::NONE, writer.bytesWritten()};
}

DecodeResult decodeOne(std::span<const std::byte> input) {
    if (input.size() < kHeaderSize) {
        return {
            DecodeStatus::NEED_MORE
        };
    }

    Header header = readHeader(input.subspan<0, 4>());

    if (header.version != kProtocolVersion) {
        return DecodeResult {
            DecodeStatus::ERROR,
            CodecError::UNSUPPORTED_VERSION,
        };
    }

    const auto expectedBodySize = bodySize(header.msg_type);

    if (!expectedBodySize) {
        return DecodeResult {
            DecodeStatus::ERROR,
            CodecError::UNKNOWN_MESSAGE_TYPE,
        };
    }

    if (header.body_len != *expectedBodySize) {
        return DecodeResult {
            DecodeStatus::ERROR,
            CodecError::INVALID_BODY_LENGTH,
        };
    }

    const std::size_t frameSize = kHeaderSize + *expectedBodySize;

    if (input.size() < frameSize) {
        return {
            DecodeStatus::NEED_MORE
        };
    }

    ByteReader reader{
        input.subspan(kHeaderSize, *expectedBodySize)
    };

    switch (header.msg_type) {
    case MessageType::HEARTBEAT: {
        Heartbeat message{};
        message.ts_ns = reader.u64();

        return {
            DecodeStatus::COMPLETE,
            CodecError::NONE,
            frameSize,
            Message{message}
        };
    }

    case MessageType::QUOTE: {
        Quote message{};
        reader.symbol(message.symbol);
        message.ts_ns = reader.u64();
        message.bid_qty = reader.u32();
        message.bid_px = reader.i64();
        message.ask_qty = reader.u32();
        message.ask_px = reader.i64();

        return {
            DecodeStatus::COMPLETE,
            CodecError::NONE,
            frameSize,
            Message{message}
        };
    }

    case MessageType::TRADE: {
        Trade message{};
        reader.symbol(message.symbol);
        message.ts_ns = reader.u64();
        message.qty = reader.u32();
        message.px = reader.i64();
        message.aggressor =
            static_cast<Aggressor>(reader.character());
        message.id = reader.i64();

        if (message.aggressor != Aggressor::BUY &&
            message.aggressor != Aggressor::SELL &&
            message.aggressor != Aggressor::UNKNOWN) {
            return {
                DecodeStatus::ERROR,
                CodecError::INVALID_FIELD,
                0,
                std::nullopt
            };
        }

        return {
            DecodeStatus::COMPLETE,
            CodecError::NONE,
            frameSize,
            Message{message}
        };
    }

    case MessageType::SESSION_CONTROL: {
        SessionControl message{};
        message.ts_ns = reader.u64();
        message.state = static_cast<SessionState>(reader.u8());

        if (message.state != SessionState::OPEN &&
            message.state != SessionState::HALT &&
            message.state != SessionState::CLOSE) {
            return {
                DecodeStatus::ERROR,
                CodecError::INVALID_FIELD,
                0,
                std::nullopt
            };
        }

        return {
            DecodeStatus::COMPLETE,
            CodecError::NONE,
            frameSize,
            Message{message}
        };
    }

    case MessageType::INVALID:
        break;
    }

    return {
        DecodeStatus::ERROR,
        CodecError::UNKNOWN_MESSAGE_TYPE,
        0,
        std::nullopt
    };
}