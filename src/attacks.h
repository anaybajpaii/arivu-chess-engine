#pragma once

#include "bitboard.h"
#include "types.h"

enum Direction : int {
    NORTH,
    NORTH_EAST,
    EAST,
    SOUTH_EAST,
    SOUTH,
    SOUTH_WEST,
    WEST,
    NORTH_WEST,
    DIRECTION_NB
};

extern Bitboard RAYS[DIRECTION_NB][64];
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[COLOR_NB][64];

void init_attacks();

inline Bitboard positive_ray_attacks(Direction d, Square s, Bitboard occ) {
    Bitboard attacks = RAYS[d][s];
    Bitboard blockers = attacks & occ;
    if (blockers) attacks ^= RAYS[d][lsb(blockers)];
    return attacks;
}

inline Bitboard negative_ray_attacks(Direction d, Square s, Bitboard occ) {
    Bitboard attacks = RAYS[d][s];
    Bitboard blockers = attacks & occ;
    if (blockers) attacks ^= RAYS[d][msb(blockers)];
    return attacks;
}

inline Bitboard rook_attacks(Square s, Bitboard occ) {
    return positive_ray_attacks(NORTH, s, occ) | positive_ray_attacks(EAST, s, occ) |
           negative_ray_attacks(SOUTH, s, occ) | negative_ray_attacks(WEST, s, occ);
}

inline Bitboard bishop_attacks(Square s, Bitboard occ) {
    return positive_ray_attacks(NORTH_EAST, s, occ) | positive_ray_attacks(NORTH_WEST, s, occ) |
           negative_ray_attacks(SOUTH_EAST, s, occ) | negative_ray_attacks(SOUTH_WEST, s, occ);
}

inline Bitboard queen_attacks(Square s, Bitboard occ) {
    return rook_attacks(s, occ) | bishop_attacks(s, occ);
}