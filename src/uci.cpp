#include "uci.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "movegen.h"
#include "search.h"

namespace {

constexpr long long DEFAULT_SEARCH_MS = 5000;

SearchLimits parse_go(const std::string& args, Color side) {
    std::istringstream in(args);
    int depth = 0;
    int moves_to_go = 0;
    long long movetime = 0;
    long long wtime = -1;
    long long btime = -1;
    long long winc = 0;
    long long binc = 0;

    for (std::string token; in >> token;) {
        if (token == "depth") {
            in >> depth;
        } else if (token == "movetime") {
            in >> movetime;
        } else if (token == "wtime") {
            in >> wtime;
        } else if (token == "btime") {
            in >> btime;
        } else if (token == "winc") {
            in >> winc;
        } else if (token == "binc") {
            in >> binc;
        } else if (token == "movestogo") {
            in >> moves_to_go;
        }
    }

    SearchLimits limits;
    if (depth > 0) limits.max_depth = std::min(depth, MAX_DEPTH);

    long long time_left = (side == WHITE) ? wtime : btime;
    long long increment = (side == WHITE) ? winc : binc;

    if (movetime > 0) {
        limits.hard_ms = std::max(1LL, movetime * 9 / 10);
        limits.soft_ms = std::max(1LL, limits.hard_ms / 2);
    } else if (time_left >= 0) {
        long long divisor = moves_to_go > 0 ? std::min(moves_to_go, 50) : 30;
        long long reserve = std::min(100LL, time_left / 10);
        long long usable = std::max(1LL, time_left - reserve);
        long long budget = usable / divisor + increment * 3 / 4;
        budget = std::max(1LL, std::min(budget, usable / 2));
        limits.hard_ms = budget;
        limits.soft_ms = std::max(1LL, budget / 2);
    } else if (depth <= 0) {
        limits.hard_ms = DEFAULT_SEARCH_MS;
        limits.soft_ms = DEFAULT_SEARCH_MS / 2;
    }

    return limits;
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
            std::string rest;
            std::getline(ss, rest);
            SearchResult result = search(board, parse_go(rest, board.side_to_move), &out);
            out << "bestmove " << (result.found ? result.best.to_uci() : std::string("0000"))
                << "\n"
                << std::flush;
        } else if (command == "quit") {
            return;
        }
    }
}