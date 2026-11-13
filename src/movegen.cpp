#include "movegen.h"

#include <initializer_list>

#include "attacks.h"
#include "bitboard.h"

namespace {

Color opposite(Color c) { return c == WHITE ? BLACK : WHITE; }

void add_targets(MoveList& list, Square from, Bitboard targets, Bitboard enemy) {
    while (targets) {
        Square to = pop_lsb(targets);
        list.add(Move(from, to, (square_bb(to) & enemy) ? CAPTURE : QUIET));
    }
}

void add_promotions(MoveList& list, Square from, Square to, bool capture) {
    int base = capture ? PROMO_CAPTURE_KNIGHT : PROMO_KNIGHT;
    for (int i = 3; i >= 0; --i) list.add(Move(from, to, base + i));
}

void add_pawn_moves(const Board& b, MoveList& list, Color us) {
    Color them = opposite(us);
    int forward = (us == WHITE) ? 8 : -8;
    int start_rank = (us == WHITE) ? 1 : 6;
    int promo_rank = (us == WHITE) ? 7 : 0;
    Bitboard pawns = b.pieces[make_piece(us, PAWN)];
    Bitboard enemy = b.occupancy[them];

    while (pawns) {
        Square from = pop_lsb(pawns);
        Square one = from + forward;

        if (!(b.occupied & square_bb(one))) {
            if (rank_of(one) == promo_rank) {
                add_promotions(list, from, one, false);
            } else {
                list.add(Move(from, one, QUIET));
                if (rank_of(from) == start_rank) {
                    Square two = one + forward;
                    if (!(b.occupied & square_bb(two))) list.add(Move(from, two, DOUBLE_PUSH));
                }
            }
        }

        Bitboard captures = PAWN_ATTACKS[us][from] & enemy;
        while (captures) {
            Square to = pop_lsb(captures);
            if (rank_of(to) == promo_rank) {
                add_promotions(list, from, to, true);
            } else {
                list.add(Move(from, to, CAPTURE));
            }
        }

        if (b.ep_square != NO_SQUARE && (PAWN_ATTACKS[us][from] & square_bb(b.ep_square))) {
            list.add(Move(from, b.ep_square, EP_CAPTURE));
        }
    }
}

void add_piece_moves(const Board& b, MoveList& list, Color us, PieceType pt) {
    Bitboard pieces = b.pieces[make_piece(us, pt)];
    Bitboard own = b.occupancy[us];
    Bitboard enemy = b.occupancy[opposite(us)];

    while (pieces) {
        Square from = pop_lsb(pieces);
        Bitboard targets = 0;
        switch (pt) {
            case KNIGHT: targets = KNIGHT_ATTACKS[from]; break;
            case BISHOP: targets = bishop_attacks(from, b.occupied); break;
            case ROOK: targets = rook_attacks(from, b.occupied); break;
            case QUEEN: targets = queen_attacks(from, b.occupied); break;
            case KING: targets = KING_ATTACKS[from]; break;
            default: break;
        }
        add_targets(list, from, targets & ~own, enemy);
    }
}

void add_castling(const Board& b, MoveList& list, Color us) {
    Color them = opposite(us);
    int base = (us == WHITE) ? 0 : 56;
    int oo_right = (us == WHITE) ? WHITE_OO : BLACK_OO;
    int ooo_right = (us == WHITE) ? WHITE_OOO : BLACK_OOO;
    Square king_sq = base + 4;

    if (!(b.pieces[make_piece(us, KING)] & square_bb(king_sq))) return;
    if (is_square_attacked(b, king_sq, them)) return;

    Bitboard rooks = b.pieces[make_piece(us, ROOK)];

    if ((b.castling_rights & oo_right) && (rooks & square_bb(base + 7)) &&
        !(b.occupied & (square_bb(base + 5) | square_bb(base + 6))) &&
        !is_square_attacked(b, base + 5, them) && !is_square_attacked(b, base + 6, them)) {
        list.add(Move(king_sq, base + 6, KING_CASTLE));
    }

    if ((b.castling_rights & ooo_right) && (rooks & square_bb(base)) &&
        !(b.occupied & (square_bb(base + 1) | square_bb(base + 2) | square_bb(base + 3))) &&
        !is_square_attacked(b, base + 3, them) && !is_square_attacked(b, base + 2, them)) {
        list.add(Move(king_sq, base + 2, QUEEN_CASTLE));
    }
}

}  // namespace

bool is_square_attacked(const Board& b, Square s, Color by) {
    Color other = opposite(by);
    if (PAWN_ATTACKS[other][s] & b.pieces[make_piece(by, PAWN)]) return true;
    if (KNIGHT_ATTACKS[s] & b.pieces[make_piece(by, KNIGHT)]) return true;
    if (KING_ATTACKS[s] & b.pieces[make_piece(by, KING)]) return true;

    Bitboard diagonal = b.pieces[make_piece(by, BISHOP)] | b.pieces[make_piece(by, QUEEN)];
    if (bishop_attacks(s, b.occupied) & diagonal) return true;

    Bitboard straight = b.pieces[make_piece(by, ROOK)] | b.pieces[make_piece(by, QUEEN)];
    return (rook_attacks(s, b.occupied) & straight) != 0;
}

bool in_check(const Board& b, Color side) {
    Square king_sq = lsb(b.pieces[make_piece(side, KING)]);
    return is_square_attacked(b, king_sq, opposite(side));
}

void generate_pseudo_legal(const Board& b, MoveList& list) {
    Color us = b.side_to_move;
    add_pawn_moves(b, list, us);
    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) add_piece_moves(b, list, us, pt);
    add_castling(b, list, us);
}