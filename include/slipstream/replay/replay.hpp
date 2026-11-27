#pragma once

#include "slipstream/replay/csv.hpp"

#include <vector>
#include <cstdint>
#include <chrono>
#include <cstddef>
#include <ostream>
#include <istream>
#include <functional>

namespace slipstream::replay {
    using ReplayWait = std::function<bool(std::chrono::steady_clock::time_point)>;

    std::vector<std::uint64_t> replayOffsetsNs(const std::vector<CsvRow>& rows);

    std::size_t replayRows(const std::vector<CsvRow>& rows, MessageType selection, 
        std::ostream& output, std::chrono::system_clock::time_point replay_start,
        bool paced = true, const ReplayWait& wait = {});

    std::vector<NewOrder> readNewOrders(std::istream& input);
}