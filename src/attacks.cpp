#include "attacks.h"

Bitboard RAYS[DIRECTION_NB][64];
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[COLOR_NB][64];

namespace {

constexpr int DIR_FILE[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int DIR_RANK[8] = {1, 1, 0, -1, -1, -1, 0, 1};
constexpr int KNIGHT_DF[8] = {1, 2, 2, 1, -1, -2, -2, -1};
constexpr int KNIGHT_DR[8] = {2, 1, -1, -2, -2, -1, 1, 2};

bool on_board(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

Bitboard jumps(Square s, const int (&df)[8], const int (&dr)[8]) {
    Bitboard result = 0;
    for (int i = 0; i < 8; ++i) {
        int f = file_of(s) + df[i];
        int r = rank_of(s) + dr[i];
        if (on_board(f, r)) result |= square_bb(make_square(f, r));
    }
    return result;
}

}  // namespace

void init_attacks() {
    for (Square s = 0; s < 64; ++s) {
        for (int d = 0; d < DIRECTION_NB; ++d) {
            Bitboard ray = 0;
            int f = file_of(s) + DIR_FILE[d];
            int r = rank_of(s) + DIR_RANK[d];
            while (on_board(f, r)) {
                ray |= square_bb(make_square(f, r));
                f += DIR_FILE[d];
                r += DIR_RANK[d];
            }
            RAYS[d][s] = ray;
        }

        KNIGHT_ATTACKS[s] = jumps(s, KNIGHT_DF, KNIGHT_DR);
        KING_ATTACKS[s] = jumps(s, DIR_FILE, DIR_RANK);

        Bitboard white_pawn = 0;
        Bitboard black_pawn = 0;
        for (int df : {-1, 1}) {
            int f = file_of(s) + df;
            if (on_board(f, rank_of(s) + 1)) white_pawn |= square_bb(make_square(f, rank_of(s) + 1));
            if (on_board(f, rank_of(s) - 1)) black_pawn |= square_bb(make_square(f, rank_of(s) - 1));
        }
        PAWN_ATTACKS[WHITE][s] = white_pawn;
        PAWN_ATTACKS[BLACK][s] = black_pawn;
    }
}