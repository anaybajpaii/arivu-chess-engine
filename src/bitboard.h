#pragma once

#include <bit>
#include <iomanip>
#include <iostream>

#include "types.h"

constexpr Bitboard square_bb(Square s) { return 1ULL << s; }

inline int popcount(Bitboard b) { return std::popcount(b); }

inline Square lsb(Bitboard b) { return std::countr_zero(b); }

inline Square msb(Bitboard b) { return 63 - std::countl_zero(b); }

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

inline void print_bitboard(Bitboard b) {
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            std::cout << ((((b >> make_square(file, rank)) & 1) != 0) ? "1 " : ". ");
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n";
    std::cout << "hex: 0x" << std::hex << std::setw(16) << std::setfill('0') << b
              << std::dec << std::setfill(' ') << "\n";
}