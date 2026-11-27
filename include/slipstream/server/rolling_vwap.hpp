#pragma once

#include "slipstream/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace slipstream::server {
    class RollingVwap {
        __extension__ using Wide = __int128;

        static_assert(std::numeric_limits<std::size_t>::digits <= 64);

        struct Entry {
            std::uint64_t ts_ns{};
            std::int64_t value{};
        };
    public:
        RollingVwap(std::uint64_t window_ns, std::size_t capacity) : window_ns_(window_ns) {
            if (window_ns == 0 || capacity == 0) {
                throw std::invalid_argument("VWAP window and capacity must be positive.");
            }

            // preallocate at construction so we dont (re)allocate during onQuote
            // as we get updates
            entries_.resize(capacity);
        }

        // amortized O(1) since 1 quote can evict multiple old quotes
        void onQuote(const Quote& quote) {
            if (quote.ts_ns >= window_ns_) {
                const auto cutoff = quote.ts_ns - window_ns_;

                // remove all entries outside the window
                while (size_ != 0 && entries_[head_].ts_ns < cutoff) {
                    sum_ -= entries_[head_].value;
                    head_ = next(head_);
                    --size_;
                }
            }

            const std::uint64_t total_qty = static_cast<std::uint64_t>(quote.bid_qty) + quote.ask_qty;

            if (total_qty == 0) {
                return;
            }

            if (size_ == entries_.size()) {
                throw std::runtime_error("VWAP capacity exceeded");
            }

            const Wide numerator = static_cast<Wide>(quote.bid_px) * quote.bid_qty
                + static_cast<Wide>(quote.ask_px) * quote.ask_qty;

            const auto quote_value = static_cast<std::int64_t>(
                numerator / static_cast<Wide>(total_qty)
            );

            entries_[tail_] = Entry {quote.ts_ns, quote_value};
            tail_ = next(tail_);
            sum_ += quote_value;
            ++size_;
        }

        [[nodiscard]] std::optional<std::int64_t> value() const noexcept {
            if (size_ == 0) {
                return std::nullopt;
            }

            return static_cast<std::int64_t>(sum_ / static_cast<Wide>(size_));
        }

        [[nodiscard]] std::uint64_t count() const noexcept {
            return static_cast<std::uint64_t>(size_);
        }

    private:
        [[nodiscard]] std::size_t next(std::size_t index) const noexcept {
            ++index;
            return index == entries_.size() ? 0 : index;
        }

        std::uint64_t window_ns_;
        std::vector<Entry> entries_;
        std::size_t head_{};
        std::size_t tail_{};
        std::size_t size_{};
        Wide sum_{};
    };
}