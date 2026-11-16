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

MoveList legal_for(const std::string& fen) {
    Board b;
    b.set_fen(fen);
    MoveList list;
    generate_legal(b, list);
    return list;
}

bool has_move(const MoveList& list, const std::string& uci, int flags) {
    for (const Move& m : list) {
        if (m.to_uci() == uci && m.flags() == flags) return true;
    }
    return false;
}

bool has_uci(const MoveList& list, const std::string& uci) {
    for (const Move& m : list) {
        if (m.to_uci() == uci) return true;
    }
    return false;
}

Move find_move(const MoveList& list, const std::string& uci) {
    for (const Move& m : list) {
        if (m.to_uci() == uci) return m;
    }
    return Move();
}

int promotion_count(const MoveList& list) {
    int n = 0;
    for (const Move& m : list) {
        if (m.is_promotion()) ++n;
    }
    return n;
}

std::string fen_after(const std::string& fen, const std::string& uci) {
    Board b;
    b.set_fen(fen);
    MoveList list;
    generate_legal(b, list);
    b.make_move(find_move(list, uci));
    return b.to_fen();
}

bool roundtrip(const std::string& fen) {
    Board b;
    b.set_fen(fen);
    Board original = b;
    MoveList first;
    generate_legal(b, first);
    for (const Move& m : first) {
        Undo u1 = b.make_move(m);
        Board after_first = b;
        MoveList second;
        generate_legal(b, second);
        for (const Move& r : second) {
            Undo u2 = b.make_move(r);
            b.unmake_move(r, u2);
            if (!(b == after_first)) return false;
        }
        b.unmake_move(m, u1);
        if (!(b == original)) return false;
    }
    return true;
}

const char* KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
const char* POSITION_3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
const char* POSITION_4 = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
const char* POSITION_5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
const char* MIDDLEGAME = "r4rk1/1pp1qppp/p1np1n2/2b1pb2/4P3/2NP1N2/PPP1BPPP/R2Q1RK1 w - - 0 10";

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

    check(fen_after(START_FEN, "e2e4") ==
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
          "e2e4 sets the en passant square");
    check(fen_after("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1") ==
              "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1",
          "short castling moves the rook and clears white rights");
    check(fen_after("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1c1") ==
              "r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1",
          "long castling moves the rook and clears white rights");
    check(fen_after("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "a1a8") ==
              "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1",
          "capturing a rook clears both affected castling rights");
    check(fen_after("8/P7/8/8/8/8/8/k6K w - - 0 1", "a7a8q") == "Q7/8/8/8/8/8/8/k6K b - - 0 1",
          "promotion replaces the pawn with a queen");
    check(fen_after("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3", "d4e3") ==
              "rnbqkbnr/ppp1pppp/8/8/8/4p3/PPPP1PPP/RNBQKBNR w KQkq - 0 4",
          "en passant removes the captured pawn");

    check(roundtrip(START_FEN), "make/unmake roundtrip from the start position");
    check(roundtrip(KIWIPETE), "make/unmake roundtrip on Kiwipete");
    check(roundtrip(POSITION_3), "make/unmake roundtrip on position 3");
    check(roundtrip(POSITION_4), "make/unmake roundtrip on position 4");
    check(roundtrip(POSITION_5), "make/unmake roundtrip on position 5");
    check(roundtrip(MIDDLEGAME), "make/unmake roundtrip on a middlegame position");
    check(roundtrip("8/P7/8/8/8/8/8/k6K w - - 0 1"), "make/unmake roundtrip with promotions");
    check(roundtrip("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3"),
          "make/unmake roundtrip with en passant");

    check(legal_for(START_FEN).count == 20, "start position has 20 legal moves");
    check(legal_for(KIWIPETE).count == 48, "Kiwipete has 48 legal moves");
    check(legal_for(POSITION_3).count == 14, "position 3 has 14 legal moves");
    check(legal_for(POSITION_4).count == 6, "position 4 has 6 legal moves");
    check(legal_for(POSITION_5).count == 44, "position 5 has 44 legal moves");
    check(legal_for(MIDDLEGAME).count == 28, "middlegame position has 28 legal moves");

    MoveList pinned = legal_for("4k3/4r3/8/8/8/8/4N3/4K3 w - - 0 1");
    check(pinned.count == 4, "pinned knight leaves only the 4 king moves");
    check(!has_uci(pinned, "e2c3"), "pinned knight cannot move");

    MoveList evasions = legal_for("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
    check(evasions.count == 3, "rook check leaves 3 legal moves");
    check(has_uci(evasions, "e1e2"), "king can capture the undefended checking rook");

    std::string ep_pin = "8/8/8/8/k2pP2R/8/8/4K3 b - e3 0 1";
    check(has_move(moves_for(ep_pin), "d4e3", EP_CAPTURE), "pinned en passant is pseudo-legal");
    check(!has_uci(legal_for(ep_pin), "d4e3"), "en passant that exposes the king is illegal");

    MoveList start_moves = legal_for(START_FEN);
    std::cout << "\nstart position moves (" << start_moves.count << "):";
    for (const Move& m : start_moves) std::cout << ' ' << m.to_uci();
    std::cout << "\n";

    return failures;
}