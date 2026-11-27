#pragma once

#include "slipstream/server/message_handler.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

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

    class L1Handler final : public IMessageHandler {
    public:
        explicit L1Handler(std::string_view symbol) : symbol_(symbol) {}

        void onMessage(FeedRole role, const Message& message) override {
            if (role == FeedRole::MarketData) {
                const auto* quote = std::get_if<Quote>(&message);

                if (!quote) {
                    throw std::runtime_error("MD requires Quote messages");
                }

                // ignore symbols that we aren't watching for
                if (!matches(quote->symbol)) {
                    return;
                }

                book_ = {
                    quote->ts_ns,
                    makeLevel(quote->bid_px, quote->bid_qty),
                    makeLevel(quote->ask_px, quote->ask_qty)
                };
                return;
            }

            if (const auto* trade = std::get_if<Trade>(&message)) {
                if (!matches(trade->symbol)) {
                    return;
                }

                // here is where trade decision logic will go.
                // to be done in milestone M3.

                return;
            }

            if (std::holds_alternative<Heartbeat>(message)) {
                return;
            }

            throw std::runtime_error("OE requires Trade or Heartbeat messages");
        }

        [[nodiscard]] const L1Book& book() const noexcept {
            return book_;
        }
    private:
        
        // We need to ensure we only match on the substring up until
        // the first null space, since symbol is null padded
        bool matches(const char (&raw)[kSymbolSize]) const noexcept {
            const std::string_view bounded{raw, kSymbolSize};
            return bounded.substr(0, bounded.find('\0')) == symbol_;
        }

        static std::optional<BookLevel> makeLevel(std::int64_t px, std::uint32_t qty) {
            if (qty == 0) {
                return std::nullopt;
            }
            return BookLevel{px, qty};
        }

        std::string symbol_;
        L1Book book_{};
    };
}