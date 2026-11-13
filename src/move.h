#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "types.h"

enum MoveFlag : int {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    PROMO_KNIGHT = 8,
    PROMO_BISHOP = 9,
    PROMO_ROOK = 10,
    PROMO_QUEEN = 11,
    PROMO_CAPTURE_KNIGHT = 12,
    PROMO_CAPTURE_BISHOP = 13,
    PROMO_CAPTURE_ROOK = 14,
    PROMO_CAPTURE_QUEEN = 15
};

struct Move {
    std::uint16_t data = 0;

    constexpr Move() = default;
    constexpr Move(Square from, Square to, int flags)
        : data(static_cast<std::uint16_t>(from | (to << 6) | (flags << 12))) {}

    constexpr Square from() const { return data & 63; }
    constexpr Square to() const { return (data >> 6) & 63; }
    constexpr int flags() const { return data >> 12; }
    constexpr bool is_capture() const { return (flags() & 4) != 0; }
    constexpr bool is_promotion() const { return (flags() & 8) != 0; }
    constexpr PieceType promotion_type() const {
        return static_cast<PieceType>((flags() & 3) + KNIGHT);
    }
    constexpr bool operator==(const Move& other) const { return data == other.data; }

    std::string to_uci() const;
};

inline std::string Move::to_uci() const {
    std::string s = square_name(from()) + square_name(to());
    if (is_promotion()) s += "nbrq"[flags() & 3];
    return s;
}

struct MoveList {
    std::array<Move, 256> moves;
    int count = 0;

    void add(Move m) { moves[count++] = m; }
    const Move* begin() const { return moves.data(); }
    const Move* end() const { return moves.data() + count; }
};