#include "slipstream/codec.hpp"
#include "byte_reader.hpp"

#include <stdexcept>
#include <utility>
#include <cassert>
#include <bit>

DecodeResult decodeOne(std::span<const std::byte> input) {
    if (input.size() < kHeaderSize) {
        return {
            DecodeStatus::NEED_MORE
        };
    }

    Header header = slipstream::detail::readHeader(input.subspan<0, 4>());

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

    slipstream::detail::ByteReader reader{
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

    case MessageType::NEW_ORDER: {
        NewOrder message{};
        message.client_order_id = reader.u64();
        reader.symbol(message.symbol);
        message.status = static_cast<OrderStatus>(reader.character());
        message.ts_ns = reader.u64();
        message.trade_id = reader.i64();
        message.side = static_cast<OrderSide>(reader.character());
        message.qty = reader.u32();
        message.limit_px = reader.i64();

        if (message.status != OrderStatus::ACCEPTED &&
            message.status != OrderStatus::REJECTED) {
            return {
                DecodeStatus::ERROR,
                CodecError::INVALID_FIELD,
                0,
                std::nullopt
            };
        }

        if (message.side != OrderSide::BUY &&
            message.side != OrderSide::SELL) {
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