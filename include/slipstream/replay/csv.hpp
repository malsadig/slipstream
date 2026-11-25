#pragma once

#include "slipstream/messages.hpp"

#include <string_view>
#include <vector>
#include <cstddef>
#include <array>
#include <string>
#include <istream>
#include <cstdint>

namespace slipstream::replay {
    std::vector<std::string_view> splitCsvLine(std::string_view line);

    struct CsvRow {
        std::size_t line_number{};
        std::array<std::string, 10> fields;
    };

    std::vector<CsvRow> readCsvRows(std::istream& input);

    std::uint32_t parseQuantity(std::string_view text, std::size_t line_number,
        std::string_view field_name);

    void copySymbol(std::string_view text, char (&destination)[12], std::size_t line_number);

    std::int64_t parsePrice(std::string_view text, std::size_t line_number, 
        std::string_view field_name);

    std::uint64_t parseTimeOfDayNs(std::string_view text, std::size_t line_number);

    Quote parseQuote(const CsvRow& row, std::uint64_t timestamp_ns);

    Trade parseTrade(const CsvRow& row, std::uint64_t timestamp_ns, std::int64_t trade_id);
}