#pragma once

#include <optional>
#include <cstdint>
#include <stdexcept>

namespace slipstream::server {
    class WarmupGate {
    public:
        explicit WarmupGate(std::uint64_t window_ns) : window_ns_(window_ns) {
            if (window_ns == 0) {
                throw std::invalid_argument("Warmup window must be positive");
            }
        }

        void onQuote(std::uint64_t ts_ns) noexcept {
            if (!first_ts_) {
                first_ts_ = ts_ns;
            }

            latest_ts_ = ts_ns;

            if (observed_quotes_ < 10) {
                ++observed_quotes_;
            }
        }

        [[nodiscard]] bool ready() const noexcept {
            return first_ts_.has_value() && observed_quotes_ >= 10
                && latest_ts_ >= *first_ts_ && latest_ts_ - *first_ts_ >= window_ns_;
        }
    private:
        std::uint64_t window_ns_;
        std::optional<std::uint64_t> first_ts_;
        std::uint64_t latest_ts_{};
        unsigned observed_quotes_{};
    };
}