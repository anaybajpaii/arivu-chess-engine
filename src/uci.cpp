#include "uci.h"

#include <cstddef>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "eval.h"
#include "movegen.h"

namespace {

std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

constexpr int MATE_SCORE = 100000;

std::string choose_move(Board& board) {
    MoveList list;
    generate_legal(board, list);
    if (list.count == 0) return "0000";

    int best_score = -MATE_SCORE - 1;
    std::vector<Move> best_moves;

    for (const Move& m : list) {
        Undo u = board.make_move(m);

        MoveList replies;
        generate_legal(board, replies);
        int score;
        if (replies.count == 0) {
            score = in_check(board, board.side_to_move) ? MATE_SCORE : 0;
        } else {
            score = -evaluate(board);
        }

        board.unmake_move(m, u);

        if (score > best_score) {
            best_score = score;
            best_moves.clear();
        }
        if (score == best_score) best_moves.push_back(m);
    }

    std::uniform_int_distribution<int> pick(0, static_cast<int>(best_moves.size()) - 1);
    return best_moves[pick(rng())].to_uci();
}

}  // namespace

bool parse_position(Board& board, const std::string& args) {
    std::istringstream in(args);
    std::vector<std::string> tokens;
    for (std::string token; in >> token;) tokens.push_back(token);
    if (tokens.empty()) return false;

    Board next;
    std::size_t i = 0;

    if (tokens[0] == "startpos") {
        next.set_fen(START_FEN);
        i = 1;
    } else if (tokens[0] == "fen") {
        std::string fen;
        i = 1;
        while (i < tokens.size() && tokens[i] != "moves") {
            if (!fen.empty()) fen += ' ';
            fen += tokens[i];
            ++i;
        }
        if (!next.set_fen(fen)) return false;
    } else {
        return false;
    }

    if (i < tokens.size()) {
        if (tokens[i] != "moves") return false;
        for (++i; i < tokens.size(); ++i) {
            MoveList list;
            generate_legal(next, list);
            bool found = false;
            for (const Move& m : list) {
                if (m.to_uci() == tokens[i]) {
                    next.make_move(m);
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    board = next;
    return true;
}

void uci_loop(std::istream& in, std::ostream& out) {
    Board board;
    board.set_fen(START_FEN);

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string command;
        if (!(ss >> command)) continue;

        if (command == "uci") {
            out << "id name Arivu\n"
                << "id author anaybajpaii\n"
                << "uciok\n"
                << std::flush;
        } else if (command == "isready") {
            out << "readyok\n" << std::flush;
        } else if (command == "ucinewgame") {
            board.set_fen(START_FEN);
        } else if (command == "position") {
            std::string rest;
            std::getline(ss, rest);
            if (!parse_position(board, rest)) {
                out << "info string invalid position command\n" << std::flush;
            }
        } else if (command == "go") {
            out << "bestmove " << choose_move(board) << "\n" << std::flush;
        } else if (command == "quit") {
            return;
        }
    }
}