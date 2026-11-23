#include "eval.h"

#include <algorithm>
#include <array>

#include "bitboard.h"

namespace {

constexpr std::array<int, PIECE_TYPE_NB> PIECE_VALUE = {100, 320, 330, 500, 900, 0};
constexpr int MAX_PHASE = 6400;

int file_centrality(Square s) { return std::min(file_of(s), 7 - file_of(s)); }

int rank_centrality(Square s) { return std::min(rank_of(s), 7 - rank_of(s)); }

int centrality(Square s) { return file_centrality(s) + rank_centrality(s); }

int piece_square_value(PieceType pt, Square s) {
    int rank = rank_of(s);
    switch (pt) {
        case PAWN:
            return (rank == 0 || rank == 7) ? 0 : 5 * (rank - 1) + 2 * file_centrality(s);
        case KNIGHT: return 10 * centrality(s) - 30;
        case BISHOP: return 5 * centrality(s) - 15;
        case ROOK: return 2 * file_centrality(s) + (rank == 6 ? 15 : 0);
        case QUEEN: return 3 * centrality(s) - 9;
        default: return 0;
    }
}

int king_middlegame_value(Square s) {
    int file = file_of(s);
    int rank = rank_of(s);
    int castled_bonus = (rank == 0 && (file == 1 || file == 2 || file == 6)) ? 10 : 0;
    return -20 * rank + castled_bonus;
}

int king_endgame_value(Square s) { return 8 * centrality(s) - 24; }

struct Tables {
    std::array<std::array<int, 64>, PIECE_TYPE_NB> piece{};
    std::array<int, 64> king_middlegame{};
    std::array<int, 64> king_endgame{};
};

const Tables TABLES = [] {
    Tables t;
    for (Square s = 0; s < 64; ++s) {
        for (int pt = PAWN; pt <= QUEEN; ++pt) {
            t.piece[pt][s] = piece_square_value(static_cast<PieceType>(pt), s);
        }
        t.king_middlegame[s] = king_middlegame_value(s);
        t.king_endgame[s] = king_endgame_value(s);
    }
    return t;
}();

}  // namespace

int evaluate(const Board& b) {
    int material[COLOR_NB] = {0, 0};
    int king_mg[COLOR_NB] = {0, 0};
    int king_eg[COLOR_NB] = {0, 0};
    int phase = 0;

    for (int c = WHITE; c < COLOR_NB; ++c) {
        Color color = static_cast<Color>(c);

        for (int pt = PAWN; pt <= QUEEN; ++pt) {
            Bitboard pieces = b.pieces[make_piece(color, static_cast<PieceType>(pt))];
            while (pieces) {
                Square s = pop_lsb(pieces);
                Square relative = (color == WHITE) ? s : (s ^ 56);
                material[c] += PIECE_VALUE[pt] + TABLES.piece[pt][relative];
                if (pt != PAWN) phase += PIECE_VALUE[pt];
            }
        }

        Bitboard king = b.pieces[make_piece(color, KING)];
        if (king) {
            Square s = lsb(king);
            Square relative = (color == WHITE) ? s : (s ^ 56);
            king_mg[c] = TABLES.king_middlegame[relative];
            king_eg[c] = TABLES.king_endgame[relative];
        }
    }

    phase = std::min(phase, MAX_PHASE);

    int total[COLOR_NB];
    for (int c = WHITE; c < COLOR_NB; ++c) {
        total[c] = material[c] + (king_mg[c] * phase + king_eg[c] * (MAX_PHASE - phase)) / MAX_PHASE;
    }

    int diff = total[WHITE] - total[BLACK];
    return b.side_to_move == WHITE ? diff : -diff;
}