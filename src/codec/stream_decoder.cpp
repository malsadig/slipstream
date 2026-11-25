#include "slipstream/stream_decoder.hpp"

StreamDecodeResult StreamDecoder::feed(std::span<const std::byte> input) {
    if (error_ != CodecError::NONE) {
        return {error_, {}};
    }

    for (auto byte : input) {
        pending_.push_back(byte);
    }

    auto consumed {0uz};
    StreamDecodeResult result{};

    while (consumed < pending_.size()) {
        const auto decoded = decodeOne(
            std::span<const std::byte>{pending_}.subspan(consumed)
        );

        if (decoded.status == DecodeStatus::COMPLETE) {
            result.messages.push_back(*decoded.message);
            consumed += decoded.bytes_consumed;
        } else if (decoded.status == DecodeStatus::NEED_MORE) {
            break;
        } else {
            error_ = decoded.error;
            result.error = decoded.error;
            break;
        }
    }

    pending_.erase(
        pending_.begin(),
        pending_.begin() + consumed
    );

    return result;
}

CodecError StreamDecoder::finish() {
    if (error_ == CodecError::NONE && !pending_.empty()) {
        error_ = CodecError::TRUNCATED_INPUT;
    }
    return error_;
}