#include "slipstream/replay/csv.hpp"

#include <algorithm>
#include <stdexcept>
#include <charconv>

constexpr std::string_view CSV_HEADER{"Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty,Aggressor"};

namespace slipstream::replay {
    std::vector<std::string_view> splitCsvLine(std::string_view line) {
        std::vector<std::string_view> fields;
        fields.reserve(10);

        while (true) {
            const auto pos = line.find(',');

            fields.emplace_back(line.substr(0, pos));

            if (pos == std::string_view::npos) {
                break;
            }

            line.remove_prefix(pos + 1);
        }

        return fields;
    }

    std::vector<CsvRow> readCsvRows(std::istream& input) {
        std::vector<CsvRow> rows{};
        std::size_t rowNum{};
        std::string line{};

        bool headerFound{false};

        while (std::getline(input, line)) {
            ++rowNum;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty() || line.starts_with("#")) {
                continue;
            }

            if (!headerFound) {
                if (line != CSV_HEADER) {
                    throw std::runtime_error(
                        "Unexpected CSV header at line " + std::to_string(rowNum)
                    );
                } else {
                    headerFound = true;
                    continue;
                }
            }

            auto fields = splitCsvLine(line);
            if (fields.size() != 10) {
                throw std::runtime_error(
                    "Invalid CSV at line " + std::to_string(rowNum)
                );
            }

            auto& row = rows.emplace_back();
            row.line_number = rowNum;

            std::copy(fields.begin(), fields.end(), row.fields.begin());
        }

        if (input.bad() || !input.eof()) {
            throw std::runtime_error("Failed to read CSV input");
        }

        if (!headerFound) { // otherwise fully empty/comment files succeed
            throw std::runtime_error("CSV header missing");
        }

        return rows;
    }

    std::uint32_t parseQuantity(std::string_view text, std::size_t line_number, std::string_view field_name) {
        const auto invalid = [&] {
            throw std::runtime_error(
                "Invalid " + std::string(field_name) +
                " at line " + std::to_string(line_number));
        };

        if (text.empty()) {
            invalid();
        }

        std::uint32_t num{};
        auto result = std::from_chars(text.begin(), text.end(), num);
        if (result.ptr != text.end() || result.ec != std::errc{}) {
            invalid();
        }
        return num;
    }

    void copySymbol(std::string_view text, char (&destination)[12], std::size_t line_number) {
        if (text.empty() || text.size() > 12) {
            throw std::runtime_error("Invalid Symbol at line " + std::to_string(line_number));
        }

        std::fill_n(destination, 12, '\0');
        std::copy(text.begin(), text.end(), destination);
    }

    std::int64_t parsePrice(std::string_view text, std::size_t line_number, std::string_view field_name) {
        const auto invalid = [&] {
            throw std::runtime_error(
                "Invalid " + std::string(field_name) +
                " at line " + std::to_string(line_number));
        };

        const bool negative = text.starts_with('-');

        if (negative || text.starts_with('+')) {
            text.remove_prefix(1);
        }

        if (text.empty()) {
            invalid();
        }

        // Split at the decimal point.
        const auto decimalPos = text.find('.');
        const auto whole = text.substr(0, decimalPos);
        const auto fraction = decimalPos == std::string_view::npos ? std::string_view{} : text.substr(decimalPos + 1);

        const auto isDigit = [](char c) {
            return c >= '0' && c <= '9';
        };

        if (whole.empty() || !std::ranges::all_of(whole, isDigit)) {
            invalid();
        }

        if (decimalPos != std::string_view::npos && (fraction.empty() || fraction.size() > 4 || !std::ranges::all_of(fraction, isDigit))) {
            invalid();
        }

        std::string scaled;

        scaled.reserve(whole.size() + 4 + negative);

        if (negative) {
            scaled.push_back('-');
        }

        scaled.append(whole);
        scaled.append(fraction);
        scaled.append(4 - fraction.size(), '0');

        std::int64_t result{};

        const auto end = scaled.data() + scaled.size();
        const auto [ptr, ec] = std::from_chars(scaled.data(), end, result);

        if (ec != std::errc{} || ptr != end) {
            invalid();
        }

        return result;
    }

    std::uint64_t parseTimeOfDayNs(std::string_view text, std::size_t line_number) {
        const auto invalid = [&] {
            throw std::runtime_error("Invalid time of day at line " + std::to_string(line_number));
        };

        if (text.size() != 12 || text.at(2) != ':' || text.at(5) != ':' || text.at(8) != '.') {
            invalid();
        }

        std::uint32_t hours { parseQuantity(text.substr(0, 2), line_number, "Timestamp") };
        std::uint32_t minutes { parseQuantity(text.substr(3, 2), line_number, "Timestamp") };
        std::uint32_t seconds { parseQuantity(text.substr(6, 2), line_number, "Timestamp") };
        std::uint32_t milliseconds { parseQuantity(text.substr(9, 3), line_number, "Timestamp") };

        if (hours > 23 || minutes > 59 || seconds > 59 || milliseconds > 999) {
            invalid();
        }

        std::uint64_t totalSeconds { (hours * 60ULL + minutes) * 60ULL + seconds };
        std::uint64_t nanoseconds { totalSeconds * 1'000'000'000ULL + milliseconds * 1'000'000ULL };

        return nanoseconds;
    }

    Quote parseQuote(const CsvRow& row, std::uint64_t timestamp_ns) {
        if (row.fields[1] != "Q") {
            throw std::runtime_error("This is not a valid Quote on line number " + std::to_string(row.line_number));
        }

        Quote quote{};

        copySymbol(row.fields[2], quote.symbol, row.line_number);
        quote.bid_qty = parseQuantity(row.fields[4], row.line_number, "BidQty");
        quote.bid_px = parsePrice(row.fields[3], row.line_number, "BidPrice");

        quote.ask_qty = parseQuantity(row.fields[6], row.line_number, "AskQty");
        quote.ask_px = parsePrice(row.fields[5], row.line_number, "AskPrice");

        quote.ts_ns = timestamp_ns;

        return quote;
    }

    Trade parseTrade(const CsvRow& row, std::uint64_t timestamp_ns, std::int64_t trade_id) {
        if (row.fields[1] != "T") {
            throw std::runtime_error("This is not a valid Trade on line number " + std::to_string(row.line_number));
        }

        Trade trade{};

        copySymbol(row.fields[2], trade.symbol, row.line_number);
        trade.ts_ns = timestamp_ns;

        trade.qty = parseQuantity(row.fields[8], row.line_number, "Qty");
        trade.px = parsePrice(row.fields[7], row.line_number, "Price");
        trade.id = trade_id;

        if (row.fields[9] == "B") {
            trade.aggressor = Aggressor::BUY;
        } else if (row.fields[9] == "S") {
            trade.aggressor = Aggressor::SELL;
        } else if (row.fields[9] == "?") {
            trade.aggressor = Aggressor::UNKNOWN;
        } else {
            throw std::runtime_error("Invalid aggressor for Trade on line number " + std::to_string(row.line_number));
        }

        return trade;
    }
}