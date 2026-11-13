#include <initializer_list>
#include <iostream>
#include <string>

#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"
#include "movegen.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << "\n";
    if (!ok) ++failures;
}

Square sq(const char* name) { return make_square(name[0] - 'a', name[1] - '1'); }

Bitboard bb(std::initializer_list<const char*> names) {
    Bitboard result = 0;
    for (const char* n : names) result |= square_bb(sq(n));
    return result;
}

MoveList moves_for(const std::string& fen) {
    Board b;
    b.set_fen(fen);
    MoveList list;
    generate_pseudo_legal(b, list);
    return list;
}

bool has_move(const MoveList& list, const std::string& uci, int flags) {
    for (const Move& m : list) {
        if (m.to_uci() == uci && m.flags() == flags) return true;
    }
    return false;
}

int promotion_count(const MoveList& list) {
    int n = 0;
    for (const Move& m : list) {
        if (m.is_promotion()) ++n;
    }
    return n;
}

}  // namespace

int main() {
    init_attacks();

    check(KNIGHT_ATTACKS[sq("e4")] == bb({"d6", "f6", "c5", "g5", "c3", "g3", "d2", "f2"}),
          "knight on e4");
    check(KNIGHT_ATTACKS[sq("a1")] == bb({"b3", "c2"}), "knight on a1");
    check(KING_ATTACKS[sq("a1")] == bb({"a2", "b1", "b2"}), "king on a1");
    check(popcount(KING_ATTACKS[sq("e4")]) == 8, "king on e4 has 8 squares");
    check(PAWN_ATTACKS[WHITE][sq("e4")] == bb({"d5", "f5"}), "white pawn on e4");
    check(PAWN_ATTACKS[WHITE][sq("a2")] == bb({"b3"}), "white pawn on a2 (edge)");
    check(PAWN_ATTACKS[BLACK][sq("h7")] == bb({"g6"}), "black pawn on h7 (edge)");

    check(rook_attacks(sq("d4"), bb({"d6", "b4"})) ==
              bb({"d5", "d6", "d3", "d2", "d1", "e4", "f4", "g4", "h4", "c4", "b4"}),
          "rook on d4 with blockers on d6 and b4");
    check(bishop_attacks(sq("d4"), bb({"f6"})) ==
              bb({"e5", "f6", "c5", "b6", "a7", "e3", "f2", "g1", "c3", "b2", "a1"}),
          "bishop on d4 with blocker on f6");
    check(popcount(rook_attacks(sq("a1"), 0)) == 14, "rook on a1, empty board");

    Board start;
    start.set_fen(START_FEN);
    check(is_square_attacked(start, sq("e3"), WHITE), "e3 attacked by white at start");
    check(!is_square_attacked(start, sq("e4"), WHITE), "e4 not attacked by white at start");
    check(is_square_attacked(start, sq("e6"), BLACK), "e6 attacked by black at start");
    check(!in_check(start, WHITE), "white not in check at start");

    Board checked;
    checked.set_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
    check(in_check(checked, WHITE), "white in check from rook on e2");

    check(moves_for(START_FEN).count == 20, "start position has 20 pseudo-legal moves");
    check(moves_for("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1").count == 26,
          "castling test position has 26 moves");

    MoveList promo = moves_for("8/P7/8/8/8/8/8/k6K w - - 0 1");
    check(promo.count == 7, "promotion position has 7 moves");
    check(promotion_count(promo) == 4, "pawn on a7 has 4 promotions");
    check(has_move(promo, "a7a8q", PROMO_QUEEN), "a7a8q is generated");

    MoveList blocked = moves_for("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    check(!has_move(blocked, "e1g1", KING_CASTLE), "no short castle through attacked f1");
    check(has_move(blocked, "e1c1", QUEEN_CASTLE), "long castle still allowed");

    MoveList ep = moves_for("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3");
    check(has_move(ep, "d4e3", EP_CAPTURE), "en passant d4e3 is generated");

    MoveList start_moves = moves_for(START_FEN);
    std::cout << "\nstart position moves (" << start_moves.count << "):";
    for (const Move& m : start_moves) std::cout << ' ' << m.to_uci();
    std::cout << "\n";

    return failures;
}