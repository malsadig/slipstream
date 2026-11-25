#pragma once

#include "slipstream/codec.hpp"

#include <cstddef>
#include <span>
#include <vector>

struct StreamDecodeResult {
    CodecError error{CodecError::NONE};
    std::vector<Message> messages;
};

class StreamDecoder {
public:
    [[nodiscard]] StreamDecodeResult feed(
        std::span<const std::byte> input);
    
    [[nodiscard]] CodecError finish();

private:
    std::vector<std::byte> pending_;
    CodecError error_{CodecError::NONE};
};