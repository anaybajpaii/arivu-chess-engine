#include "uci.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "movegen.h"
#include "search.h"
#include "tt.h"

namespace {

constexpr long long DEFAULT_SEARCH_MS = 5000;
constexpr std::size_t DEFAULT_HASH_MB = 16;
constexpr std::size_t MIN_HASH_MB = 1;
constexpr std::size_t MAX_HASH_MB = 1024;

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

bool parse_setoption(const std::string& args, std::string& name, std::string& value) {
    std::istringstream in(args);
    std::string token;
    if (!(in >> token) || token != "name") return false;

    name.clear();
    value.clear();
    bool reading_value = false;
    while (in >> token) {
        if (!reading_value && token == "value") {
            reading_value = true;
        } else if (reading_value) {
            if (!value.empty()) value += ' ';
            value += token;
        } else {
            if (!name.empty()) name += ' ';
            name += token;
        }
    }
    return !name.empty();
}

bool parse_hash_size(const std::string& text, std::size_t& megabytes) {
    if (text.empty() || text.size() > 6) return false;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
    }
    std::size_t value = static_cast<std::size_t>(std::stoul(text));
    if (value < MIN_HASH_MB || value > MAX_HASH_MB) return false;
    megabytes = value;
    return true;
}

}  // namespace

bool parse_position(Board& board, const std::string& args, std::vector<std::uint64_t>& history) {
    std::istringstream in(args);
    std::vector<std::string> tokens;
    for (std::string token; in >> token;) tokens.push_back(token);
    if (tokens.empty()) return false;

    Board next;
    std::vector<std::uint64_t> next_history;
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
                    next_history.push_back(next.hash);
                    next.make_move(m);
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    board = next;
    history = std::move(next_history);
    return true;
}

void uci_loop(std::istream& in, std::ostream& out) {
    Board board;
    board.set_fen(START_FEN);
    std::vector<std::uint64_t> history;
    TranspositionTable tt(DEFAULT_HASH_MB);

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string command;
        if (!(ss >> command)) continue;

        if (command == "uci") {
            out << "id name Arivu\n"
                << "id author anaybajpaii\n"
                << "option name Hash type spin default " << DEFAULT_HASH_MB << " min "
                << MIN_HASH_MB << " max " << MAX_HASH_MB << "\n"
                << "option name Clear Hash type button\n"
                << "uciok\n"
                << std::flush;
        } else if (command == "isready") {
            out << "readyok\n" << std::flush;
        } else if (command == "ucinewgame") {
            board.set_fen(START_FEN);
            history.clear();
            tt.clear();
        } else if (command == "setoption") {
            std::string rest;
            std::getline(ss, rest);
            std::string name;
            std::string value;
            if (!parse_setoption(rest, name, value)) {
                out << "info string invalid setoption command\n" << std::flush;
            } else if (name == "Hash") {
                std::size_t megabytes = 0;
                if (parse_hash_size(value, megabytes)) {
                    tt.resize(megabytes);
                } else {
                    out << "info string invalid Hash value\n" << std::flush;
                }
            } else if (name == "Clear Hash") {
                tt.clear();
            } else {
                out << "info string unknown option " << name << "\n" << std::flush;
            }
        } else if (command == "position") {
            std::string rest;
            std::getline(ss, rest);
            if (!parse_position(board, rest, history)) {
                out << "info string invalid position command\n" << std::flush;
            }
        } else if (command == "go") {
            std::string rest;
            std::getline(ss, rest);
            SearchResult result = search(board, parse_go(rest, board.side_to_move), &out, history, &tt);
            out << "bestmove " << (result.found ? result.best.to_uci() : std::string("0000"))
                << "\n"
                << std::flush;
        } else if (command == "quit") {
            return;
        }
    }
}
