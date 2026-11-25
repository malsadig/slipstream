#include "slipstream/messages.hpp"
#include "slipstream/codec.hpp"
#include "byte_writer.hpp"

#include <stdexcept>
#include <utility>

EncodeResult encode(const Heartbeat& message, std::span<std::byte> output) {
    if (output.size() < kHeartbeatWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    slipstream::detail::ByteWriter writer{output, MessageType::HEARTBEAT};

    writer.header();
    writer.u64(message.ts_ns);

    return {CodecError::NONE, writer.bytesWritten()};
}

EncodeResult encode(const Quote& message, std::span<std::byte> output) {
    if (output.size() < kQuoteWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    slipstream::detail::ByteWriter writer{output, MessageType::QUOTE};

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

    if (message.aggressor != Aggressor::BUY &&
        message.aggressor != Aggressor::SELL &&
        message.aggressor != Aggressor::UNKNOWN) {
        return {CodecError::INVALID_FIELD, 0};
    }

    slipstream::detail::ByteWriter writer{output, MessageType::TRADE};

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

    if (message.state != SessionState::OPEN &&
        message.state != SessionState::HALT &&
        message.state != SessionState::CLOSE) {
        return {CodecError::INVALID_FIELD, 0};
    }

    slipstream::detail::ByteWriter writer{output, MessageType::SESSION_CONTROL};

    writer.header();
    writer.u64(message.ts_ns);
    writer.u8(std::to_underlying(message.state));

    return {CodecError::NONE, writer.bytesWritten()};
}

EncodeResult encode(const NewOrder& message, std::span<std::byte> output) {
    if (output.size() < kNewOrderWireSize) {
        return {CodecError::BUFFER_TOO_SMALL, 0};
    }

    if (message.status != OrderStatus::ACCEPTED &&
        message.status != OrderStatus::REJECTED) {
        return {CodecError::INVALID_FIELD, 0};
    }

    if (message.side != OrderSide::BUY &&
        message.side != OrderSide::SELL) {
        return {CodecError::INVALID_FIELD, 0};
    }

    slipstream::detail::ByteWriter writer{output, MessageType::NEW_ORDER};

    writer.header();
    writer.u64(message.client_order_id);
    writer.symbol(message.symbol);
    writer.character(std::to_underlying(message.status));
    writer.u64(message.ts_ns);
    writer.i64(message.trade_id);
    writer.character(std::to_underlying(message.side));
    writer.u32(message.qty);
    writer.i64(message.limit_px);

    return {CodecError::NONE, writer.bytesWritten()};
}