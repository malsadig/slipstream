#include "slipstream/replay/replay.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 4 && (argc != 5 || std::string_view(argv[4]) != "--no-wait")) {
        std::cerr << "Usage: oe_client <input.csv> <trades.bin> <responses.bin> [--no-wait]\n";
        return 1;
    }

    bool paced{argc == 4};

    try {
        std::ifstream input{argv[1]};

        if (!input) {
            throw std::runtime_error("Failure opening input " + std::string(argv[1]));
        }

        std::ifstream responses{argv[3], std::ios::binary};

        if (!responses) {
            throw std::runtime_error("Failure opening responses " + std::string(argv[3]));
        }

        const auto rows = slipstream::replay::readCsvRows(input);

        const auto orders = slipstream::replay::readNewOrders(responses);

        std::ofstream output(argv[2], std::ios::binary);

        if (!output) {
            throw std::runtime_error("Failure opening output " + std::string(argv[2]));
        }

        const auto start = std::chrono::system_clock::now();

        const auto emitted = slipstream::replay::replayRows(
            rows, MessageType::TRADE, output, start, paced
        );

        std::cerr << "Emitted trade count: " << emitted << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
