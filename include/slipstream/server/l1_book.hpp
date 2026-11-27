#pragma once

#include <optional>
#include <cstdint>

namespace slipstream::server {
    struct BookLevel {
        std::int64_t px{};
        std::uint32_t qty{};
    };

    struct L1Book {
        std::uint64_t ts_ns{};
        std::optional<BookLevel> bid;
        std::optional<BookLevel> ask;
    };
}