#pragma once

#include "slipstream/replay/csv.hpp"

#include <vector>
#include <cstdint>
#include <chrono>
#include <cstddef>
#include <ostream>

namespace slipstream::replay {
    std::vector<std::uint64_t> replayOffsetsNs(const std::vector<CsvRow>& rows);

    std::size_t replayRows(const std::vector<CsvRow>& rows, MessageType selection, 
        std::ostream& output, std::chrono::system_clock::time_point replay_start, bool paced = true);
}