#include "slipstream/replay/replay.hpp"
#include "slipstream/replay/tcp_replay.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <string>

int main(int argc, char* argv[]) {
    if (argc >= 3 && std::string_view{argv[2]} == "--tcp") {
        return slipstream::replay::runTcpReplay(
            argc, argv, MessageType::QUOTE
        );
    }
    if (argc != 3 && (argc != 4 || std::string_view(argv[3]) != "--no-wait")) {
        std::cerr
            << "Usage:\n"
            << "  md_client <input.csv> <output.bin> [--no-wait]\n"
            << "  md_client <input.csv> --tcp <host> <port> "
            "--start-at-ns <unix-ns>" << '\n';
        return 1;
    }

    bool paced{argc == 3};

    try {
        std::ifstream input{argv[1]};

        if (!input) {
            throw std::runtime_error("Failure opening input " + std::string(argv[1]));
        }

        const auto rows = slipstream::replay::readCsvRows(input);

        std::ofstream output(argv[2], std::ios::binary);

        if (!output) {
            throw std::runtime_error("Failure opening output " + std::string(argv[2]));
        }

        const auto start = std::chrono::system_clock::now();

        const auto emitted = slipstream::replay::replayRows(
            rows, MessageType::QUOTE, output, start, paced
        );

        std::cerr << "Emitted quote count: " << emitted << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}