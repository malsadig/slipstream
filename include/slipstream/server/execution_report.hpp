#pragma once

#include "slipstream/server/decision_engine.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace slipstream::server {
    inline void printExecutionReport(
        std::ostream& out,
        std::string_view symbol,
        const DecisionEngine& decisions,
        std::uint32_t cap_x10000
    ) {
        const auto market = decisions.marketQty();
        const auto accepted = decisions.acceptedQty();
        const auto benchmark = decisions.sessionVwap();

        // Exact formatting of a fixed-point price, including INT64_MIN.
        const auto printPrice = [&](std::optional<std::int64_t> px) {
            if (!px) {
                out << "N/A";
                return;
            }

            const auto magnitude = *px < 0
                ? std::uint64_t{0} - static_cast<std::uint64_t>(*px)
                : static_cast<std::uint64_t>(*px);

            if (*px < 0) {
                out << '-';
            }

            out << magnitude / 10'000 << '.'
                << std::setfill('0') << std::setw(4)
                << magnitude % 10'000
                << std::setfill(' ');
        };

        const auto printParticipation = [&] {
            if (market == 0) {
                out << "N/A";
            } else {
                out << 100.0L * static_cast<long double>(accepted)
                        / static_cast<long double>(market)
                    << '%';
            }
        };

        out << std::dec << std::fixed << std::setprecision(2)
            << "\n=== SLIPSTREAM EXECUTION REPORT ===\n"
            << "symbol              " << symbol << '\n'
            << "basis               accepted historical trades; "
            "market qty is summed quoted size\n"
            << "market qty          " << market << '\n'
            << "executed qty        " << accepted << " (";

        printParticipation();
        out << ")\n";

        out << "avg fill price      ";
        printPrice(decisions.averageAcceptedPrice());
        out << " (accepted trades)\n";

        out << "session VWAP        ";
        printPrice(benchmark);
        out << '\n';

        out << "slippage vs VWAP    ";
        if (accepted == 0 || !benchmark || *benchmark == 0) {
            out << "N/A\n";
        } else {
            const long double qty = accepted;
            const long double v = *benchmark;
            const long double signed_qty =
                2.0L * static_cast<long double>(decisions.buyQty()) - qty;

            const long double cost =
                static_cast<long double>(decisions.signedNotional())
                - v * signed_qty;

            const long double bps =
                10'000.0L * cost / (std::abs(v) * qty);

            out << bps << " bps ("
                << (bps < 0 ? "favorable"
                    : bps > 0 ? "unfavorable" : "flat")
                << "; side-adjusted)\n";
        }

        out << "participation       ";
        printParticipation();
        out << " (cap "
            << static_cast<long double>(cap_x10000) / 100.0L
            << "%)\n"
            << "tick-to-order p50   N/A (M4)\n"
            << "tick-to-order p99   N/A (M4)\n"
            << "tick-to-order p99.9 N/A (M4)\n";

        out.flush();
        if (!out) {
            throw std::runtime_error("Cannot print execution report");
        }
    }

}