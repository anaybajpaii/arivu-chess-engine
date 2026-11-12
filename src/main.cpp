#include <iostream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "board.h"

int main() {
    const std::vector<std::string> good = {
        START_FEN,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
    };

    const std::vector<std::string> bad = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "hello",
    };

    int failures = 0;

    for (const std::string& fen : good) {
        Board b;
        bool ok = b.set_fen(fen) && b.to_fen() == fen;
        std::cout << (ok ? "PASS round-trip: " : "FAIL round-trip: ") << fen << "\n";
        if (!ok) ++failures;
    }

    for (const std::string& fen : bad) {
        Board b;
        bool rejected = !b.set_fen(fen);
        std::cout << (rejected ? "PASS rejected:   " : "FAIL accepted:   ") << fen << "\n";
        if (!rejected) ++failures;
    }

    Board start;
    start.set_fen(START_FEN);
    std::cout << "\n";
    start.print();
    std::cout << "\nwhite occupancy:\n";
    print_bitboard(start.occupancy[WHITE]);
    std::cout << "white pieces: " << popcount(start.occupancy[WHITE]) << "\n";

    return failures;
}