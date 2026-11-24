#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

#include "board.h"
#include "move.h"

constexpr int MATE_SCORE = 30000;
constexpr int INF_SCORE = 32000;
constexpr int MAX_DEPTH = 64;

struct SearchLimits {
    int max_depth = MAX_DEPTH;
    long long soft_ms = 0;
    long long hard_ms = 0;
};

struct SearchResult {
    bool found = false;
    Move best;
    int score = 0;
    int depth = 0;
    std::uint64_t nodes = 0;
};

SearchResult search(Board& board, const SearchLimits& limits, std::ostream* info,
                    const std::vector<std::uint64_t>& history = {});