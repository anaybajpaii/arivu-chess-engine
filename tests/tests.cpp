#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "uci.h"
#include "zobrist.h"

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

std::uint64_t perft_of(const std::string& fen, int depth) {
    Board b;
    b.set_fen(fen);
    return perft(b, depth);
}

void check_perft(const std::string& name, const std::string& fen, int depth,
                 std::uint64_t expected) {
    std::uint64_t got = perft_of(fen, depth);
    check(got == expected, "perft " + name + " depth " + std::to_string(depth) + " = " +
                               std::to_string(expected));
}

std::string run_uci(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    uci_loop(in, out);
    return out.str();
}

std::string bestmove_of(const std::string& output) {
    std::size_t pos = output.find("bestmove ");
    if (pos == std::string::npos) return "";
    std::istringstream ss(output.substr(pos + 9));
    std::string move;
    ss >> move;
    return move;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool random_games_are_legal(int games, int max_plies) {
    for (int g = 0; g < games; ++g) {
        std::string moves;
        for (int ply = 0; ply < max_plies; ++ply) {
            std::string command = "position startpos";
            if (!moves.empty()) command += " moves" + moves;
            std::string output = run_uci(command + "\ngo depth 2\n");
            std::string move = bestmove_of(output);
            if (contains(output, "info string") || move.empty()) return false;
            if (move == "0000") break;
            moves += " " + move;
        }
    }
    return true;
}

std::string mirror_fen(const std::string& fen) {
    std::istringstream in(fen);
    std::string placement, side, castling, ep, rest;
    in >> placement >> side >> castling >> ep;
    std::getline(in, rest);

    std::vector<std::string> ranks;
    std::string current;
    for (char c : placement) {
        if (c == '/') {
            ranks.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    ranks.push_back(current);

    auto swap_case = [](char c) {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isupper(u)) return static_cast<char>(std::tolower(u));
        if (std::islower(u)) return static_cast<char>(std::toupper(u));
        return c;
    };

    std::string mirrored;
    for (std::size_t i = ranks.size(); i-- > 0;) {
        for (char c : ranks[i]) mirrored += swap_case(c);
        if (i > 0) mirrored += '/';
    }

    side = (side == "w") ? "b" : "w";
    for (char& c : castling) c = swap_case(c);
    if (ep != "-") ep[1] = static_cast<char>('1' + '8' - ep[1]);

    return mirrored + ' ' + side + ' ' + castling + ' ' + ep + rest;
}

int eval_of(const std::string& fen) {
    Board b;
    b.set_fen(fen);
    return evaluate(b);
}

bool eval_is_mirror_symmetric(const std::string& fen) {
    return eval_of(fen) == eval_of(mirror_fen(fen));
}

bool random_positions_are_mirror_symmetric(int games, int plies) {
    std::mt19937 gen(12345);
    for (int g = 0; g < games; ++g) {
        Board b;
        b.set_fen(START_FEN);
        for (int ply = 0; ply < plies; ++ply) {
            if (!eval_is_mirror_symmetric(b.to_fen())) return false;
            MoveList list;
            generate_legal(b, list);
            if (list.count == 0) break;
            std::uniform_int_distribution<int> pick(0, list.count - 1);
            b.make_move(list.moves[pick(gen)]);
        }
    }
    return true;
}

bool is_noisy_move(Move m) {
    return m.is_capture() || (m.is_promotion() && m.promotion_type() == QUEEN);
}

int reference_quiescence(Board& b, int ply, int alpha, int beta) {
    bool checked = in_check(b, b.side_to_move);
    MoveList legal;
    generate_legal(b, legal);

    if (checked) {
        if (legal.count == 0) return -MATE_SCORE + ply;
        if (b.halfmove_clock >= 100) return 0;
    } else if (b.halfmove_clock >= 100) {
        return 0;
    }

    int best = checked ? -INF_SCORE : evaluate(b);
    if (best >= beta) return best;
    if (best > alpha) alpha = best;

    for (const Move& m : legal) {
        if (!checked && !is_noisy_move(m)) continue;
        Undo u = b.make_move(m);
        int score = -reference_quiescence(b, ply + 1, -beta, -alpha);
        b.unmake_move(m, u);
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

int minimax(Board& b, int depth, int ply) {
    if (depth <= 0) return reference_quiescence(b, ply, -INF_SCORE, INF_SCORE);

    MoveList list;
    generate_legal(b, list);
    if (list.count == 0) return in_check(b, b.side_to_move) ? -MATE_SCORE + ply : 0;
    if (b.halfmove_clock >= 100) return 0;

    int best = -INF_SCORE;
    for (const Move& m : list) {
        Undo u = b.make_move(m);
        int score = -minimax(b, depth - 1, ply + 1);
        b.unmake_move(m, u);
        if (score > best) best = score;
    }
    return best;
}

bool search_matches_minimax(const std::string& fen, int depth) {
    Board b;
    b.set_fen(fen);
    Board original = b;

    SearchLimits limits;
    limits.max_depth = depth;
    SearchResult result = search(b, limits, nullptr);

    bool unchanged = (b == original);
    int expected = minimax(b, depth, 0);
    return result.found && result.depth == depth && result.score == expected && unchanged;
}

std::string timed_uci(const std::string& input, long long& elapsed_ms) {
    auto begin = std::chrono::steady_clock::now();
    std::string output = run_uci(input);
    auto end = std::chrono::steady_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    return output;
}

bool hash_is_consistent(Board& b, int depth) {
    if (b.hash != b.compute_hash()) return false;
    if (depth == 0) return true;
    MoveList list;
    generate_legal(b, list);
    for (const Move& m : list) {
        Board before = b;
        Undo u = b.make_move(m);
        bool ok = hash_is_consistent(b, depth - 1);
        b.unmake_move(m, u);
        if (!ok || !(b == before)) return false;
    }
    return true;
}

bool random_games_keep_hash_in_sync(int games, int max_plies) {
    std::mt19937 rng(12345);
    for (int g = 0; g < games; ++g) {
        Board b;
        b.set_fen(START_FEN);
        for (int ply = 0; ply < max_plies; ++ply) {
            if (b.hash != b.compute_hash()) return false;
            Board reloaded;
            reloaded.set_fen(b.to_fen());
            if (reloaded.hash != b.hash) return false;
            MoveList list;
            generate_legal(b, list);
            if (list.count == 0) break;
            b.make_move(list.moves[rng() % static_cast<unsigned>(list.count)]);
        }
    }
    return true;
}

std::uint64_t hash_after(const std::string& position_args) {
    Board b;
    std::vector<std::uint64_t> history;
    if (!parse_position(b, position_args, history)) return 0;
    return b.hash;
}

bool all_keys_are_distinct_and_nonzero() {
    std::set<std::uint64_t> seen;
    std::size_t expected = 0;
    for (const auto& per_piece : zobrist::KEYS.piece) {
        for (std::uint64_t key : per_piece) {
            seen.insert(key);
            ++expected;
        }
    }
    for (std::uint64_t key : zobrist::KEYS.castling) {
        seen.insert(key);
        ++expected;
    }
    for (std::uint64_t key : zobrist::KEYS.ep_file) {
        seen.insert(key);
        ++expected;
    }
    seen.insert(zobrist::KEYS.side);
    ++expected;
    return seen.size() == expected && seen.count(0) == 0;
}

std::vector<std::uint64_t> history_that_repeats_every_reply(const std::string& fen) {
    Board b;
    b.set_fen(fen);
    MoveList list;
    generate_legal(b, list);
    std::vector<std::uint64_t> history;
    for (const Move& m : list) {
        Undo u = b.make_move(m);
        history.push_back(0x0123456789ABCDEFULL);
        history.push_back(b.hash);
        b.unmake_move(m, u);
    }
    return history;
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

    check_perft("start", START_FEN, 1, 20);
    check_perft("start", START_FEN, 2, 400);
    check_perft("start", START_FEN, 3, 8902);
    check_perft("start", START_FEN, 4, 197281);
    check_perft("start", START_FEN, 5, 4865609);

    check_perft("Kiwipete", KIWIPETE, 1, 48);
    check_perft("Kiwipete", KIWIPETE, 2, 2039);
    check_perft("Kiwipete", KIWIPETE, 3, 97862);
    check_perft("Kiwipete", KIWIPETE, 4, 4085603);

    check_perft("position 3", POSITION_3, 1, 14);
    check_perft("position 3", POSITION_3, 2, 191);
    check_perft("position 3", POSITION_3, 3, 2812);
    check_perft("position 3", POSITION_3, 4, 43238);
    check_perft("position 3", POSITION_3, 5, 674624);

    check_perft("position 4", POSITION_4, 1, 6);
    check_perft("position 4", POSITION_4, 2, 264);
    check_perft("position 4", POSITION_4, 3, 9467);
    check_perft("position 4", POSITION_4, 4, 422333);

    check_perft("position 5", POSITION_5, 1, 44);
    check_perft("position 5", POSITION_5, 2, 1486);
    check_perft("position 5", POSITION_5, 3, 62379);
    check_perft("position 5", POSITION_5, 4, 2103487);

    check_perft("middlegame", MIDDLEGAME, 1, 28);
    check_perft("middlegame", MIDDLEGAME, 2, 1335);
    check_perft("middlegame", MIDDLEGAME, 3, 38160);
    check_perft("middlegame", MIDDLEGAME, 4, 1727073);

    std::string handshake = run_uci("uci\nisready\nquit\n");
    check(contains(handshake, "id name Arivu"), "uci replies with the engine name");
    check(contains(handshake, "uciok"), "uci replies uciok");
    check(contains(handshake, "readyok"), "isready replies readyok");
    check(!contains(run_uci("quit\nisready\n"), "readyok"), "nothing is processed after quit");

    Board parsed;
    std::vector<std::uint64_t> parsed_history;
    check(parse_position(parsed, " startpos moves e2e4 e7e5 g1f3", parsed_history) &&
              parsed.to_fen() == "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
          "position startpos with moves");
    check(parse_position(parsed, " fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1 moves e2e4", parsed_history) &&
              parsed.to_fen() == "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1",
          "position fen with moves");

    Board untouched;
    untouched.set_fen(START_FEN);
    Board attempt = untouched;
    check(!parse_position(attempt, " startpos moves e2e4 e2e5", parsed_history) && attempt == untouched,
          "an illegal move is rejected and leaves the board unchanged");
    check(!parse_position(attempt, " fen not a real fen", parsed_history) && attempt == untouched,
          "an invalid fen is rejected and leaves the board unchanged");

    std::string first_reply = run_uci("position startpos\ngo depth 2\n");
    check(has_uci(legal_for(START_FEN), bestmove_of(first_reply)),
          "go from the start position plays a legal move");
    check(bestmove_of(run_uci("position fen rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3\ngo depth 2\n")) == "0000",
          "go in a checkmated position replies 0000");
    check(bestmove_of(run_uci("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\ngo depth 2\n")) == "0000",
          "go in a stalemated position replies 0000");
    check(random_games_are_legal(20, 150), "20 random games play only legal moves through uci");

    check(eval_of(START_FEN) == 0, "the start position evaluates to 0");
    check(eval_is_mirror_symmetric(KIWIPETE), "Kiwipete evaluates the same when mirrored");
    check(eval_is_mirror_symmetric(POSITION_3), "position 3 evaluates the same when mirrored");
    check(eval_is_mirror_symmetric(POSITION_4), "position 4 evaluates the same when mirrored");
    check(eval_is_mirror_symmetric(POSITION_5), "position 5 evaluates the same when mirrored");
    check(eval_is_mirror_symmetric(MIDDLEGAME),
          "the middlegame position evaluates the same when mirrored");
    check(random_positions_are_mirror_symmetric(5, 100),
          "500 random game positions evaluate the same when mirrored");

    int queen_up = eval_of("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");
    check(queen_up >= 850 && queen_up <= 1000, "a queen up is worth about 900");
    check(eval_of("4k3/8/8/8/8/8/8/3QK3 b - - 0 1") == -queen_up,
          "the score flips sign with the side to move");
    check(eval_of("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1") > eval_of("4k3/8/8/8/8/8/8/N3K3 w - - 0 1"),
          "a central knight beats a corner knight");
    check(eval_of("4k3/8/4P3/8/8/8/8/4K3 w - - 0 1") > eval_of("4k3/8/8/8/8/4P3/8/4K3 w - - 0 1"),
          "an advanced pawn beats a pawn on its third rank");
    check(eval_of("7k/8/8/8/3K4/8/8/8 w - - 0 1") > eval_of("7k/8/8/8/8/8/8/K7 w - - 0 1"),
          "in the endgame the king prefers the centre");
    check(eval_of("rnbqkbnr/pppppppp/8/8/8/4K3/PPPPPPPP/RNBQ1BNR w - - 0 1") < 0,
          "in the middlegame a king that walks out is penalised");

    check(bestmove_of(run_uci("position fen 6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1\ngo depth 2\n")) == "a1a8",
          "go plays mate in one");
    check(bestmove_of(run_uci("position fen 4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1\ngo depth 2\n")) == "d1d5",
          "go captures a free queen");

    check(search_matches_minimax(START_FEN, 3), "alpha-beta matches plain minimax from the start");
    check(search_matches_minimax(POSITION_3, 4), "alpha-beta matches plain minimax on position 3");
    check(search_matches_minimax(POSITION_5, 1), "alpha-beta matches plain minimax on position 5");
    check(search_matches_minimax(MIDDLEGAME, 2),
          "alpha-beta matches plain minimax on the middlegame position");

    check(search_matches_minimax("7k/8/8/8/8/8/p5K1/8 w - - 0 1", 1),
          "alpha-beta matches plain minimax when a pawn can promote at the leaf");
    check(search_matches_minimax("8/P6k/8/8/8/8/6K1/8 b - - 0 1", 2),
          "alpha-beta matches plain minimax with a promotion at depth 2");
    {
        Board promo;
        promo.set_fen("7k/8/8/8/8/8/p5K1/8 w - - 0 1");
        SearchLimits limits;
        limits.max_depth = 1;
        SearchResult result = search(promo, limits, nullptr);
        check(result.found && result.score < -500, "quiescence sees the promotion that follows");
    }

    check(bestmove_of(run_uci("position fen 4k3/8/4p3/3p4/8/8/8/3QK3 w - - 0 1\ngo depth 1\n")) !=
              "d1d5",
          "quiescence stops a depth 1 search from grabbing a defended pawn");

    std::string depth_run = run_uci("position startpos\ngo depth 3\n");
    check(contains(depth_run, "info depth 3 ") && !contains(depth_run, "info depth 4 "),
          "go depth 3 stops at depth 3");

    std::string mate_one = run_uci("position fen 6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1\ngo depth 3\n");
    check(contains(mate_one, "score mate 1") && bestmove_of(mate_one) == "a1a8",
          "search reports mate in 1 and plays it");

    std::string mate_two = run_uci("position fen 7k/8/8/8/8/8/1R6/R6K w - - 0 1\ngo depth 5\n");
    std::string mate_two_move = bestmove_of(mate_two);
    check(contains(mate_two, "score mate 2") &&
              (mate_two_move == "b2b7" || mate_two_move == "a1a7" || mate_two_move == "a1g1"),
          "search finds mate in 2");
    check(!contains(mate_two, "info depth 4 "), "search stops once a mate is found");

    std::string mated = run_uci("position fen 8/8/8/2R5/8/6K1/7R/5k2 b - - 0 1\ngo depth 3\n");
    check(contains(mated, "score mate -1"), "search reports being mated in 1");

    check(bestmove_of(run_uci("position fen 4k3/8/4p3/3p4/8/8/8/3QK3 w - - 0 1\ngo depth 3\n")) !=
              "d1d5",
          "search does not take a defended pawn with the queen");

    std::string forced = run_uci("position fen 7k/1R6/8/8/8/8/8/R6K b - - 0 1\ngo depth 5\n");
    check(bestmove_of(forced) == "h8g8" && !contains(forced, "info depth"),
          "a single legal move is played without searching");

    Board fifty;
    fifty.set_fen("4k3/8/8/8/8/8/8/3QK3 w - - 99 80");
    SearchLimits fifty_limits;
    fifty_limits.max_depth = 2;
    check(search(fifty, fifty_limits, nullptr).score == 0, "search scores the fifty-move rule as a draw");


    {
        Board h;
        h.set_fen(START_FEN);
        check(h.hash != 0 && h.hash == h.compute_hash(), "the start position has a non-zero hash");
        check(all_keys_are_distinct_and_nonzero(), "all zobrist keys are distinct and non-zero");

        for (const char* fen : {START_FEN, KIWIPETE, POSITION_3, POSITION_4, POSITION_5, MIDDLEGAME}) {
            Board b;
            b.set_fen(fen);
            check(hash_is_consistent(b, 3),
                  std::string("incremental hash matches a full recompute to depth 3 from ") + fen);
        }
        check(random_games_keep_hash_in_sync(30, 200),
              "hash stays in sync with a recompute and a fen reload in random games");

        check(hash_after(" startpos moves g1f3 g8f6 b1c3 b8c6") ==
                  hash_after(" startpos moves b1c3 b8c6 g1f3 g8f6"),
              "transpositions reach the same hash");
        check(hash_after(" startpos moves g1f3 g8f6 f3g1 f6g8") == hash_after(" startpos"),
              "returning to the start position restores the hash");
        check(hash_after(" startpos moves e2e4") != hash_after(" startpos moves d2d4"),
              "different positions get different hashes");
        check(hash_after(" fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1") !=
                  hash_after(" fen 4k3/8/8/8/8/8/8/4K3 b - - 0 1"),
              "the side to move changes the hash");
        check(hash_after(" fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1") !=
                  hash_after(" fen r3k2r/8/8/8/8/8/8/R3K2R w Kkq - 0 1"),
              "castling rights change the hash");
        check(hash_after(" fen 4k3/8/8/8/4Pp2/8/8/4K3 b - e3 0 1") !=
                  hash_after(" fen 4k3/8/8/8/4Pp2/8/8/4K3 b - - 0 1"),
              "an en passant square changes the hash");
        check(hash_after(" fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1") ==
                  hash_after(" fen 4k3/8/8/8/8/8/8/4K3 w - - 37 90"),
              "the move counters do not change the hash");

        Board capture;
        capture.set_fen("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
        Board before = capture;
        MoveList list;
        generate_legal(capture, list);
        bool restored = true;
        for (const Move& m : list) {
            Undo u = capture.make_move(m);
            capture.unmake_move(m, u);
            if (capture.hash != before.hash) restored = false;
        }
        check(restored, "unmake restores the hash");

        Board with_history;
        std::vector<std::uint64_t> history;
        check(parse_position(with_history, " startpos moves g1f3 g8f6 f3g1 f6g8", history) &&
                  history.size() == 4 && history[0] == with_history.hash,
              "position records the hash of every earlier position");
        check(parse_position(with_history, " startpos", history) && history.empty(),
              "position without moves has no history");
    }

    {
        const char* winning = "4k3/8/8/8/8/8/8/3QK3 w - - 90 80";
        Board b;
        b.set_fen(winning);
        SearchLimits limits;
        limits.max_depth = 1;
        int plain = search(b, limits, nullptr).score;
        int repeated = search(b, limits, nullptr, history_that_repeats_every_reply(winning)).score;
        check(plain > 500, "without history the queen is worth its value");
        check(repeated == 0, "a move that repeats an earlier position is scored as a draw");
    }

    {
        Board perpetual;
        perpetual.set_fen("6k1/8/5P2/5qp1/4Qr2/8/8/6K1 w - - 0 1");
        SearchLimits limits;
        limits.max_depth = 6;
        SearchResult result = search(perpetual, limits, nullptr);
        check(result.found && result.best.to_uci() == "e4e8" && result.score >= -50 &&
                  result.score <= 50,
              "search finds the perpetual check instead of losing");
    }

    {
        std::vector<std::uint64_t> shuffle_history;
        Board shuffle;
        check(parse_position(shuffle, " startpos moves g1f3 g8f6 f3g1", shuffle_history) &&
                  has_uci(legal_for("rnbqkb1r/pppppppp/5n2/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 2 2"),
                          "f6g8") &&
                  shuffle_history.size() == 3,
              "a shuffled opening position is parsed with its history");
        SearchLimits limits;
        limits.max_depth = 4;
        SearchResult result = search(shuffle, limits, nullptr, shuffle_history);
        check(result.found && has_uci(legal_for(shuffle.to_fen()), result.best.to_uci()),
              "search with game history returns a legal move");
    }

    {
        std::string base = "position fen 4k3/8/8/8/8/8/8/3QK3 w - - 0 1";
        std::string shuffled = run_uci(base + " moves d1d2 e8e7 d2d1\ngo depth 3\n");
        std::string fresh = run_uci("position fen 4k3/8/8/8/8/8/8/3QK3 b - - 3 1\ngo depth 3\n");
        check(bestmove_of(shuffled) == "e7e8" && contains(shuffled, "score cp 0 "),
              "uci passes the game history so the engine repeats the position to draw");
        check(!contains(fresh, "score cp 0 "), "the same kind of position without history is not a draw");
    }

    long long elapsed = 0;
    std::string timed = timed_uci("position startpos\ngo movetime 200\n", elapsed);
    check(has_uci(legal_for(START_FEN), bestmove_of(timed)) && elapsed < 1000,
          "go movetime 200 answers in time with a legal move");
    timed = timed_uci(std::string("position fen ") + KIWIPETE + "\ngo movetime 100\n", elapsed);
    check(has_uci(legal_for(KIWIPETE), bestmove_of(timed)) && elapsed < 400,
          "go movetime 100 on a complex position stops mid-iteration");
    timed = timed_uci("position startpos\ngo wtime 1000 btime 1000\n", elapsed);
    check(has_uci(legal_for(START_FEN), bestmove_of(timed)) && elapsed < 500,
          "go with 1 second on the clock answers quickly");
    timed = timed_uci("position startpos\ngo wtime 30 btime 30\n", elapsed);
    check(has_uci(legal_for(START_FEN), bestmove_of(timed)) && elapsed < 500,
          "go with 30 ms on the clock still answers with a legal move");

    std::cout << "\n" << (failures == 0 ? "all tests passed" : "some tests failed") << "\n";
    return failures;
}