#include "slipstream/replay/replay.hpp"
#include "slipstream/codec.hpp"
#include "slipstream/stream_decoder.hpp"

#include <stdexcept>
#include <string>
#include <array>
#include <thread>
#include <variant>
#include <iostream>

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

            // TODO: Remove the following when slipstream server is implemented

            std::string line = selection == MessageType::QUOTE ? "[MD]" : "[OE]";

            line += " replay_ts_ns=" + std::to_string(timestamp_ns);

            if (selection == MessageType::TRADE) {
                line += " trade_id=" + std::to_string(idx + 1);
            }

            line += " source_csv=";
            for (std::size_t column = 0; column < row.fields.size(); ++column) {
                if (column != 0) {
                    line += ',';
                }
                line += row.fields[column];
            }
            line += '\n';

            std::cerr.write(
                line.data(),
                static_cast<std::streamsize>(line.size())
            );
        }

        if (paced && !offsets.empty()) {
            std::this_thread::sleep_until(
                steady_start + std::chrono::nanoseconds(offsets.back())
            );
        }

        output.flush();
        if (!output) {
            throw std::runtime_error("Error in output after loop.");
        }

        return emittedFrames;
    }

    std::vector<NewOrder> readNewOrders(std::istream& input) {
        StreamDecoder decoder{};
        std::vector<NewOrder> newOrders{};
        std::array<std::byte, 64> buffer{};

        while (input) {
            input.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size())
            );
            const auto count = input.gcount();

            if (count > 0) {
                auto result = decoder.feed(
                    std::span<const std::byte>{buffer}.first(
                        static_cast<std::size_t>(count)
                    )
                );

                if (result.error != CodecError::NONE) {
                    throw std::runtime_error("Decoder error.");
                }

                for (auto& message : result.messages) {
                    const NewOrder* newOrder = std::get_if<NewOrder>(&message);
                    if (newOrder != nullptr) {
                        newOrders.push_back(*newOrder);
                    } else {
                        throw std::runtime_error("Expected a NewOrder response.");
                    }
                }
            }
        }

        if (input.bad() || !input.eof()) {
            throw std::runtime_error("Error after input read loop.");
        }

        if (decoder.finish() != CodecError::NONE) {
            throw std::runtime_error("Error with decoder finish");
        }

        return newOrders;
    }
}