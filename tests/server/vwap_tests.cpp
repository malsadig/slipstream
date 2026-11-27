#include "slipstream/server/l1_handler.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <array>
#include <string_view>
#include <vector>

using slipstream::server::FeedRole;
using slipstream::server::L1Handler;
using slipstream::server::RollingVwap;

static Quote makeQuote(std::uint64_t ts, std::int64_t bid_px, std::uint32_t bid_qty, 
    std::int64_t ask_px, std::uint32_t ask_qty, std::string_view symbol = "SYNTH1") {
    assert(symbol.size() <= kSymbolSize);

    Quote quote{};

    std::copy(symbol.begin(), symbol.end(), quote.symbol);
    quote.ts_ns = ts;
    quote.bid_px = bid_px;
    quote.bid_qty = bid_qty;
    quote.ask_px = ask_px;
    quote.ask_qty = ask_qty;

    return quote;
}

class CapturingReports final : public slipstream::server::IReportSink {
public:
    std::vector<slipstream::server::ReportSnapshot> reports;

    void publish(
        const slipstream::server::ReportSnapshot& report
    ) override {
        reports.push_back(report);
    }
};

int main() {
    {
        L1Handler handler{"SYNTH1", 10, 4};
        assert(!handler.vwap().value());

        handler.onMessage(
            FeedRole::MarketData,
            makeQuote(100, 200000, 200, 210000, 300)
        );

        assert(handler.vwap().value() == 206000);

        handler.onMessage(
            FeedRole::MarketData,
            makeQuote(101, 220000, 250, 250000, 400)
        );

        assert(handler.vwap().value() == 222230);
        assert(handler.vwap().count() == 2);

        // None of these messages should change the book or advance the VWAP clock.
        const auto assertUnchanged = [&] {
            assert(handler.vwap().value() == 222230);
            assert(handler.vwap().count() == 2);
            assert(handler.book().ts_ns == 101);

            assert(handler.book().bid);
            assert(handler.book().bid->px == 220000);
            assert(handler.book().bid->qty == 250);

            assert(handler.book().ask);
            assert(handler.book().ask->px == 250000);
            assert(handler.book().ask->qty == 400);
        };

        const std::array<std::string_view, 4> otherSymbols{
            "SYNTH2", "SYNTH3", "SYNTH4", "SYNTH5"
        };

        for (const auto symbol : otherSymbols) {
            handler.onMessage(
                FeedRole::MarketData,
                makeQuote(1000, 900000, 100, 910000, 200, symbol)
            );
            assertUnchanged();
        }

        Trade trade{};
        constexpr std::string_view selectedSymbol{"SYNTH1"};
        std::copy(selectedSymbol.begin(), selectedSymbol.end(), trade.symbol);
        trade.ts_ns = 1000;
        trade.px = 990000;
        trade.qty = 1000;

        handler.onMessage(FeedRole::OrderEntry, trade);
        assertUnchanged();

        handler.onMessage(FeedRole::OrderEntry, Heartbeat{1000});
        assertUnchanged();
    }

    {
        RollingVwap window{10, 2};
        assert(!window.value());
        assert(window.count() == 0);

        // Timestamp below the window length must not underflow the cutoff.
        window.onQuote(makeQuote(0, 10, 1, 0, 0));

        // Timestamp 0 is exactly at the cutoff, so both observations remain.
        window.onQuote(makeQuote(10, 20, 1, 0, 0));
        assert(window.value() == 15);
        assert(window.count() == 2);

        // A cancellation contributes nothing and must work even when full.
        window.onQuote(makeQuote(10, 999, 0, 999, 0));
        assert(window.value() == 15);
        assert(window.count() == 2);

        bool rejected = false;
        try {
            window.onQuote(makeQuote(10, 30, 1, 0, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
        assert(window.value() == 15);
        assert(window.count() == 2);

        // Evict timestamp 0, then reuse its slot through wraparound.
        window.onQuote(makeQuote(11, 30, 1, 0, 0));
        assert(window.value() == 25);
        assert(window.count() == 2);

        // Cancellation advances time; timestamp 11 remains at the cutoff.
        window.onQuote(makeQuote(21, 0, 0, 0, 0));
        assert(window.value() == 30);
        assert(window.count() == 1);

        window.onQuote(makeQuote(22, 0, 0, 0, 0));
        assert(!window.value());
        assert(window.count() == 0);

        // Reuse the emptied ring. A contributing zero price is available.
        window.onQuote(makeQuote(22, 0, 1, 0, 0));
        assert(window.value().has_value());
        assert(window.value() == 0);
        assert(window.count() == 1);

        // Also exercise an ask-only contribution and equal timestamps.
        window.onQuote(makeQuote(22, 0, 0, 40, 1));
        assert(window.value() == 20);
        assert(window.count() == 2);

        // One update must remove every expired observation.
        window.onQuote(makeQuote(33, 0, 0, 0, 0));
        assert(!window.value());
        assert(window.count() == 0);
    }

    {
        constexpr auto minPrice = std::numeric_limits<std::int64_t>::min();
        constexpr auto maxPrice = std::numeric_limits<std::int64_t>::max();
        constexpr auto maxQty = std::numeric_limits<std::uint32_t>::max();

        // Products, quantity totals, and rolling sums exceed 64-bit bounds.
        for (const auto price : std::array{minPrice, maxPrice}) {
            RollingVwap window{10, 2};

            window.onQuote(makeQuote(100, price, maxQty, price, maxQty));
            assert(window.value() == price);
            assert(window.count() == 1);

            window.onQuote(makeQuote(100, price, maxQty, price, maxQty));
            assert(window.value() == price);
            assert(window.count() == 2);
        }

        // Per-quote division: (INT64_MIN + INT64_MAX) / 2 truncates to 0.
        RollingVwap perQuote{10, 2};
        perQuote.onQuote(makeQuote(100, minPrice, maxQty, maxPrice, maxQty));
        assert(perQuote.value().has_value());
        assert(perQuote.value() == 0);
        assert(perQuote.count() == 1);

        // Final mean: two separate observations also truncate -1 / 2 to 0.
        RollingVwap mean{10, 2};
        mean.onQuote(makeQuote(100, minPrice, 1, 0, 0));
        mean.onQuote(makeQuote(100, maxPrice, 1, 0, 0));
        assert(mean.value().has_value());
        assert(mean.value() == 0);
        assert(mean.count() == 2);
    }

    {
        CapturingReports sink;
        L1Handler handler{"SYNTH1", 10, 4, &sink};

        handler.onMessage(
            FeedRole::MarketData,
            makeQuote(100, 200000, 200, 210000, 300)
        );

        assert(sink.reports.size() == 1);
        assert(sink.reports[0].ts_ns == 100);
        assert(sink.reports[0].window_quotes == 1);
        assert(sink.reports[0].has_vwap);
        assert(sink.reports[0].vwap_px == 206000);
        assert((sink.reports[0].symbol ==
                std::array<char, kSymbolSize>{'S', 'Y', 'N', 'T', 'H', '1'}));

        handler.onMessage(
            FeedRole::MarketData,
            makeQuote(1000, 900000, 1, 910000, 1, "SYNTH2")
        );

        Trade trade{};
        std::copy_n("SYNTH1", 6, trade.symbol);
        trade.ts_ns = 1000;
        handler.onMessage(FeedRole::OrderEntry, trade);
        handler.onMessage(FeedRole::OrderEntry, Heartbeat{1000});

        assert(sink.reports.size() == 1);

        // Cancellation expires the old contribution and still publishes a row.
        handler.onMessage(
            FeedRole::MarketData,
            makeQuote(111, 0, 0, 0, 0)
        );

        assert(sink.reports.size() == 2);
        assert(sink.reports[1].ts_ns == 111);
        assert(sink.reports[1].window_quotes == 0);
        assert(!sink.reports[1].has_vwap);
    }
}