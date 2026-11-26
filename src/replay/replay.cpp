#include "slipstream/replay/replay.hpp"
#include "slipstream/codec.hpp"

#include <stdexcept>
#include <string>
#include <array>
#include <thread>

namespace slipstream::replay {
    std::vector<std::uint64_t> replayOffsetsNs(const std::vector<CsvRow>& rows) {
        std::vector<std::uint64_t> offsets{};

        if (rows.empty()) {
            return offsets;
        }

        std::uint64_t origin = parseTimeOfDayNs(rows[0].fields[0], rows[0].line_number);

        offsets.reserve(rows.size());

        std::uint64_t prevTimestamp = origin;

        for (const auto& row : rows) {
            std::uint64_t timestamp = parseTimeOfDayNs(row.fields[0], row.line_number);
            if (timestamp < prevTimestamp) {
                throw std::runtime_error("Timestamp is smaller than prev timestamp on line " + std::to_string(row.line_number));
            }
            offsets.push_back(timestamp - origin);
            prevTimestamp = timestamp;
        }

        return offsets;
    }

    std::size_t replayRows(const std::vector<CsvRow>& rows, MessageType selection, 
        std::ostream& output, std::chrono::system_clock::time_point replay_start, bool paced) {
        if (selection != MessageType::QUOTE && selection != MessageType::TRADE) {
            throw std::runtime_error("Selection must be of type QUOTE or TRADE.");
        }

        auto offsets = replayOffsetsNs(rows);

        const auto start_count = std::chrono::duration_cast<std::chrono::nanoseconds>(
            replay_start.time_since_epoch()
        ).count();

        if (start_count < 0) {
            throw std::runtime_error("Cannot have a negative start count.");
        }

        std::uint64_t start = start_count;

        std::array<std::byte, kQuoteWireSize> buffer{};

        std::size_t emittedFrames{0};

        const auto wall_now = std::chrono::system_clock::now();
        const auto steady_now = std::chrono::steady_clock::now();
        const auto steady_start = steady_now + (replay_start - wall_now);

        for (auto idx{0uz}; idx < rows.size(); ++idx) {
            const auto& row = rows[idx];
            const auto& type = row.fields[1];

            if (type != "Q" && type != "T") {
                throw std::runtime_error(
                    "Invalid row type at line " + std::to_string(row.line_number));
            }

            if ((selection == MessageType::QUOTE && type == "T") ||
                (selection == MessageType::TRADE && type == "Q")) {
                continue;
            }

            const auto timestamp_ns = start + offsets[idx];

            EncodeResult result{};

            if (selection == MessageType::QUOTE) {
                const auto quote = parseQuote(row, timestamp_ns);
                result = encode(quote, buffer);
            } else {
                const auto trade = parseTrade(
                    row,
                    timestamp_ns,
                    static_cast<std::int64_t>(idx + 1)
                );
                result = encode(trade, buffer);
            }

            if (result.error != CodecError::NONE) {
                throw std::runtime_error("Error encoding row #" + std::to_string(idx));
            }

            if (paced) {
                std::this_thread::sleep_until(
                    steady_start + std::chrono::nanoseconds(offsets[idx])
                );
            }

            output.write(
                reinterpret_cast<const char*>(buffer.data()),
                static_cast<std::streamsize>(result.bytes_written)
            );

            if (paced) {
                output.flush();
            }

            if (!output) {
                throw std::runtime_error("Error emitting row #" + std::to_string(idx));
            } else {
                ++emittedFrames;
            }
        }

        output.flush();
        if (!output) {
            throw std::runtime_error("Error in output after loop.");
        }

        return emittedFrames;
    }
}