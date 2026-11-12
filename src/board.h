#pragma once

#include <array>
#include <string>

#include "types.h"

inline constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct Board {
    std::array<Bitboard, PIECE_COUNT> pieces{};
    std::array<Bitboard, COLOR_NB> occupancy{};
    Bitboard occupied = 0;
    std::array<Piece, 64> mailbox{};

    Color side_to_move = WHITE;
    int castling_rights = 0;
    Square ep_square = NO_SQUARE;
    int halfmove_clock = 0;
    int fullmove_number = 1;

    Board();

    void clear();
    void put_piece(Piece p, Square s);
    bool set_fen(const std::string& fen);
    std::string to_fen() const;
    void print() const;

private:
    bool parse_fen(const std::string& fen);
};