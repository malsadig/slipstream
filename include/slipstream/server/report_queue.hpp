#pragma once

#include "slipstream/messages.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace slipstream::server {
    struct ReportSnapshot {
        std::uint64_t ts_ns{};
        std::array<char, kSymbolSize> symbol{};
        std::uint64_t window_quotes{};
        std::int64_t vwap_px{};
        bool has_vwap{};

        bool operator==(const ReportSnapshot&) const = default;
    };

    // Exactly one producer and one consumer.
    // Both must stop using the queue before its destruction.
    class ReportQueue {
        static_assert(
            std::atomic<std::size_t>::is_always_lock_free,
            "Report queue requires lock-free index atomics"
        );

    public:
        explicit ReportQueue(std::size_t capacity) {
            if (capacity == 0 ||
                capacity == std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument("Invalid report queue capacity");
            }

            // preallocate required capacity, +1 extra internal slot to
            // distinguish full from empty
            slots_.resize(capacity + 1);
        }

        [[nodiscard]] bool tryPush(const ReportSnapshot& report) noexcept {
            const auto write = write_.load(std::memory_order_relaxed);
            const auto next_write = next(write);

            if (next_write == read_.load(std::memory_order_acquire)) {
                return false;
            }

            slots_[write] = report;
            write_.store(next_write, std::memory_order_release);
            return true;
        }

        [[nodiscard]] bool tryPop(ReportSnapshot& report) noexcept {
            const auto read = read_.load(std::memory_order_relaxed);

            if (read == write_.load(std::memory_order_acquire)) {
                return false;
            }

            report = slots_[read];
            read_.store(next(read), std::memory_order_release);
            return true;
        }

        [[nodiscard]] std::size_t capacity() const noexcept {
            return slots_.size() - 1;
        }

    private:
        [[nodiscard]] std::size_t next(std::size_t index) const noexcept {
            ++index;
            return index == slots_.size() ? 0 : index;
        }

        std::vector<ReportSnapshot> slots_;
        std::atomic<std::size_t> write_{0};
        std::atomic<std::size_t> read_{0};
    };
}