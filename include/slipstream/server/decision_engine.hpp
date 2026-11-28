#pragma once

#include "slipstream/messages.hpp"
#include "slipstream/server/l1_book.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <cstdint>

namespace slipstream::server {
    class DecisionEngine {
        __extension__ using Wide = __int128;
    public:
        DecisionEngine(
            std::uint32_t max_quantity,
            std::uint32_t band_bps_x10000,
            std::uint32_t participation_cap_x10000
        ) : max_quantity_(max_quantity), band_(band_bps_x10000), cap_(participation_cap_x10000) {
            if (max_quantity == 0 || band_ > 100'000'000 || cap_ > 10'000) {
                throw std::invalid_argument("Invalid decision settings.");
            }
        }

        void onQuote(const Quote& quote) {
            const auto added = static_cast<std::uint64_t>(quote.bid_qty) + quote.ask_qty;

            if (added > std::numeric_limits<std::uint64_t>::max() - market_qty_) {
                throw std::overflow_error("Cumulative quoted quantity overflow");
            }

            market_qty_ += added;

            if (added != 0) {
                const Wide numerator =
                    static_cast<Wide>(quote.bid_px) * quote.bid_qty
                    + static_cast<Wide>(quote.ask_px) * quote.ask_qty;

                session_sum_ += numerator / static_cast<Wide>(added);
                ++session_count_;
            }
        }

        [[nodiscard]] std::optional<OrderStatus> decide(
            const Trade& trade,
            const L1Book& book,
            std::optional<std::int64_t> vwap,
            bool enabled
        ) {
            if (!enabled) {
                return std::nullopt;
            }

            if (trade.aggressor != Aggressor::BUY && trade.aggressor != Aggressor::SELL) {
                throw std::runtime_error("Cannot decide a trade without a BUY or SELL aggressor");
            }

            if (trade.qty == 0 || trade.qty > max_quantity_ || !vwap) {
                return OrderStatus::REJECTED;
            }

            const bool buy = trade.aggressor == Aggressor::BUY;
            const auto& level = buy ? book. ask : book.bid;

            if (!level || level->qty == 0) {
                return OrderStatus::REJECTED;
            }

            constexpr Wide band_denominator = 100'000'000;

            const Wide factor = buy ? band_denominator - band_ : band_denominator + band_;

            const Wide threshold = static_cast<Wide>(*vwap) * factor;
            const Wide quoted_price = static_cast<Wide>(level->px) * band_denominator;

            const bool band_ok = buy ? quoted_price <= threshold : quoted_price >= threshold;
            const bool crosses = buy ? trade.px >= level->px : trade.px <= level->px;

            if (!band_ok || !crosses) {
                return OrderStatus::REJECTED;
            }

            const Wide candidate = static_cast<Wide>(accepted_qty_) + trade.qty;

            if (candidate * 10'000 > static_cast<Wide>(market_qty_) * cap_) {
                return OrderStatus::REJECTED;
            }

            accepted_qty_ = static_cast<std::uint64_t>(candidate);

            const Wide notional = static_cast<Wide>(trade.px) * trade.qty;

            accepted_notional_ += notional;
            signed_notional_ += buy ? notional : -notional;

            if (buy) {
                buy_qty_ += trade.qty;
            }
            return OrderStatus::ACCEPTED;
        }

        [[nodiscard]] std::uint64_t marketQty() const noexcept {
            return market_qty_;
        }

        [[nodiscard]] std::uint64_t acceptedQty() const noexcept {
            return accepted_qty_;
        }

        [[nodiscard]] std::optional<std::int64_t> averageAcceptedPrice() const noexcept {
            if (accepted_qty_ == 0) {
                return std::nullopt;
            }

            return static_cast<std::int64_t>(
                accepted_notional_ / static_cast<Wide>(accepted_qty_)
            );
        }

        [[nodiscard]] std::optional<std::int64_t> sessionVwap() const noexcept {
            if (session_count_ == 0) {
                return std::nullopt;
            }

            return static_cast<std::int64_t>(
                session_sum_ / static_cast<Wide>(session_count_)
            );
        }

        [[nodiscard]] Wide signedNotional() const noexcept {
            return signed_notional_;
        }

        [[nodiscard]] std::uint64_t buyQty() const noexcept {
            return buy_qty_;
        }
    private:
        std::uint32_t max_quantity_;
        std::uint32_t band_;
        std::uint32_t cap_;
        std::uint64_t market_qty_{};
        std::uint64_t accepted_qty_{};

        Wide session_sum_{};
        std::uint64_t session_count_{};
        Wide accepted_notional_{};
        Wide signed_notional_{}; // BUY notional minus SELL notional.
        std::uint64_t buy_qty_{};
    };
}