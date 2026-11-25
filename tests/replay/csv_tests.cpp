#include "slipstream/replay/csv.hpp"

#include <cassert>
#include <string_view>
#include <vector>

int main() {
    {
        const std::vector<std::string_view> expected{"A", "B"};
        assert(slipstream::replay::splitCsvLine("A,B") == expected);
    }
    {
        const std::vector<std::string_view> expected{"A", "", "B"};
        assert(slipstream::replay::splitCsvLine("A,,B") == expected);
    }
    {
        const std::vector<std::string_view> expected{"A", "B", ""};
        assert(slipstream::replay::splitCsvLine("A,B,") == expected);
    }
    {
        const std::vector<std::string_view> expected{"", "", ""};
        assert(slipstream::replay::splitCsvLine(",,") == expected);
    }
    {
        const std::vector<std::string_view> expected{""};
        assert(slipstream::replay::splitCsvLine("") == expected);
    }
    {
        const std::vector<std::string_view> expected{"09:30:00.003", "Q", "SYNTH3", "87.37", "55", "87.49", "50", "", "", "" };
        assert(slipstream::replay::splitCsvLine("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,") == expected);
    }
}