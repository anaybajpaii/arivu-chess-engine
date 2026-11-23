#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

#include "attacks.h"
#include "board.h"
#include "perft.h"
#include "uci.h"

namespace {

void print_usage() {
    std::cout << "usage:\n"
              << "  arivu                    (UCI mode, for chess GUIs)\n"
              << "  arivu perft <depth> [fen]\n"
              << "  arivu divide <depth> [fen]\n";
}

std::string join_args(int argc, char** argv, int first) {
    std::string result;
    for (int i = first; i < argc; ++i) {
        if (i > first) result += ' ';
        result += argv[i];
    }
    return result;
}

int run_perft(int depth, Board& board) {
    for (int d = 1; d <= depth; ++d) {
        auto begin = std::chrono::steady_clock::now();
        std::uint64_t nodes = perft(board, d);
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - begin).count();
        double mnps = ms > 0 ? static_cast<double>(nodes) / ms / 1000.0 : 0.0;
        std::cout << "depth " << d << ": " << nodes << " nodes, " << static_cast<long long>(ms)
                  << " ms, " << mnps << " Mnps\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    init_attacks();

    if (argc == 1) {
        uci_loop(std::cin, std::cout);
        return 0;
    }

    std::string command = argv[1];

    if (command == "perft" || command == "divide") {
        if (argc < 3) {
            print_usage();
            return 1;
        }

        int depth = 0;
        try {
            depth = std::stoi(argv[2]);
        } catch (const std::exception&) {
            depth = 0;
        }
        if (depth < 1) {
            std::cout << "depth must be a positive number\n";
            return 1;
        }

        std::string fen = argc > 3 ? join_args(argc, argv, 3) : std::string(START_FEN);
        Board board;
        if (!board.set_fen(fen)) {
            std::cout << "invalid fen: " << fen << "\n";
            return 1;
        }

        if (command == "divide") {
            divide(board, depth);
            return 0;
        }
        return run_perft(depth, board);
    }

    print_usage();
    return 1;
}