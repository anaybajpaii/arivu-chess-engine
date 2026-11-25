#pragma once

#include <cstddef>
#include <cstdint>

#include "move.h"

enum Bound : std::uint8_t { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

struct TTEntry {
    std::uint64_t key = 0;
    Move move;
    std::int16_t score = 0;
    std::uint8_t depth = 0;
    Bound bound = BOUND_NONE;
    std::uint8_t age = 0;
};

static_assert(sizeof(TTEntry) == 16, "a transposition table entry should fit in 16 bytes");

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes);
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    void resize(std::size_t megabytes);
    void clear();
    void new_search() { ++age_; }

    bool probe(std::uint64_t key, TTEntry& out) const;
    void store(std::uint64_t key, Move move, int score, int depth, Bound bound);

    int hashfull() const;
    std::size_t entry_count() const { return count_; }

private:
    TTEntry* entries_ = nullptr;
    std::size_t count_ = 0;
    std::size_t mask_ = 0;
    std::uint8_t age_ = 0;
};

int score_to_tt(int score, int ply);
int score_from_tt(int score, int ply);
