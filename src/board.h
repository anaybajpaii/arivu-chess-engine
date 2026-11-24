#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "move.h"
#include "types.h"

inline constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct Undo {
    Piece captured = NO_PIECE;
    int castling_rights = 0;
    Square ep_square = NO_SQUARE;
    int halfmove_clock = 0;
    std::uint64_t hash = 0;
};

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
    std::uint64_t hash = 0;

    Board();

    void clear();
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);
    bool set_fen(const std::string& fen);
    std::string to_fen() const;
    std::uint64_t compute_hash() const;
    void print() const;

    Undo make_move(Move m);
    void unmake_move(Move m, const Undo& u);

    bool operator==(const Board& other) const = default;

private:
    bool parse_fen(const std::string& fen);
};