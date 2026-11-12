#pragma once

#include <cstdint>
#include <string>

using Bitboard = std::uint64_t;
using Square = int;

enum Color : int { WHITE, BLACK, COLOR_NB };

enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_TYPE_NB };

enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE
};

constexpr int PIECE_COUNT = 12;
constexpr Square NO_SQUARE = 64;

constexpr int WHITE_OO = 1;
constexpr int WHITE_OOO = 2;
constexpr int BLACK_OO = 4;
constexpr int BLACK_OOO = 8;

constexpr Square make_square(int file, int rank) { return rank * 8 + file; }
constexpr int file_of(Square s) { return s & 7; }
constexpr int rank_of(Square s) { return s >> 3; }

constexpr Piece make_piece(Color c, PieceType pt) { return static_cast<Piece>(c * 6 + pt); }
constexpr Color color_of(Piece p) { return static_cast<Color>(p / 6); }
constexpr PieceType type_of(Piece p) { return static_cast<PieceType>(p % 6); }

inline std::string square_name(Square s) {
    return std::string{static_cast<char>('a' + file_of(s)), static_cast<char>('1' + rank_of(s))};
}