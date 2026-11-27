#pragma once

#include "slipstream/server/message_handler.hpp"
#include "slipstream/server/rolling_vwap.hpp"
#include "slipstream/server/report_sink.hpp"
#include "slipstream/server/warmup_gate.hpp"
#include "slipstream/server/l1_book.hpp"
#include "slipstream/server/decision_engine.hpp"
#include "slipstream/server/order_sink.hpp"

#include <limits>

#include <cstdint>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <cstddef>

namespace slipstream::server {

    class L1Handler final : public IMessageHandler {
    public:
        explicit L1Handler(std::string_view symbol, std::uint64_t window_ns, 
            std::size_t capacity, IReportSink* reports = nullptr,
            DecisionEngine* decisions = nullptr, IOrderSink* orders = nullptr): 
            symbol_(symbol), vwap_(window_ns, capacity), 
            warmup_(window_ns), reports_(reports), decisions_(decisions),
            orders_(orders)  {
            if ((decisions_ != nullptr) != (orders_ != nullptr)) {
                throw std::invalid_argument(
                    "Decision engine and order sink must be supplied together"
                );
            }
        }

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

                vwap_.onQuote(*quote);

                book_ = {
                    quote->ts_ns,
                    makeLevel(quote->bid_px, quote->bid_qty),
                    makeLevel(quote->ask_px, quote->ask_qty)
                };

                warmup_.onQuote(quote->ts_ns);

                if (decisions_) {
                    decisions_->onQuote(*quote);
                }

                if (reports_) {
                    const auto value = vwap_.value();
                    
                    ReportSnapshot report{};
                    report.ts_ns = book_.ts_ns;
                    std::copy_n(
                        quote->symbol,
                        report.symbol.size(),
                        report.symbol.begin()
                    );
                    report.window_quotes = vwap_.count();
                    report.vwap_px = value.value_or(0);
                    report.has_vwap = value.has_value();

                    reports_->publish(report);
                }
                return;
            }

            if (const auto* trade = std::get_if<Trade>(&message)) {
                if (!matches(trade->symbol) || !decisions_) {
                    return;
                }

                const auto status = decisions_->decide(*trade, book_, vwap_.value(), warmedUp());

                if (!status) {
                    return;
                }

                if (last_order_id_ == std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error("Order IDs exhausted");
                }

                NewOrder order{};
                order.client_order_id = ++last_order_id_;
                std::copy_n(trade->symbol, kSymbolSize, order.symbol);
                order.status = *status;
                order.trade_id = trade->id;
                order.side = trade->aggressor == Aggressor::BUY ? OrderSide::BUY : OrderSide::SELL;
                order.qty = trade->qty;
                order.limit_px = trade->px;

                orders_->send(order);

                return;
            }

            if (std::holds_alternative<Heartbeat>(message)) {
                return;
            }

            throw std::runtime_error("OE requires Trade or Heartbeat messages");
        }

        [[nodiscard]] bool warmedUp() const noexcept {
            return warmup_.ready();
        }

        [[nodiscard]] const L1Book& book() const noexcept {
            return book_;
        }

        [[nodiscard]] const RollingVwap& vwap() const noexcept {
            return vwap_;
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
        RollingVwap vwap_;
        WarmupGate warmup_;
        L1Book book_{};
        IReportSink* reports_{}; // Borrowed; must outlive the handler.
        DecisionEngine* decisions_{}; // Borrowed.
        IOrderSink* orders_{};        // Borrowed.
        std::uint64_t last_order_id_{};
    };
}