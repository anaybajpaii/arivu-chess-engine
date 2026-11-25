#include "tt.h"

#include <cstdlib>
#include <new>

#include "search.h"

namespace {

constexpr int MATE_BOUND = MATE_SCORE - 1000;

std::size_t entries_for(std::size_t megabytes) {
    std::size_t bytes = (megabytes < 1 ? 1 : megabytes) * 1024 * 1024;
    std::size_t wanted = bytes / sizeof(TTEntry);
    std::size_t count = 1;
    while (count * 2 <= wanted) count *= 2;
    return count;
}

}  // namespace

TranspositionTable::TranspositionTable(std::size_t megabytes) { resize(megabytes); }

TranspositionTable::~TranspositionTable() { std::free(entries_); }

void TranspositionTable::resize(std::size_t megabytes) {
    std::size_t count = entries_for(megabytes);
    void* memory = std::calloc(count, sizeof(TTEntry));
    if (memory == nullptr) throw std::bad_alloc();
    std::free(entries_);
    entries_ = static_cast<TTEntry*>(memory);
    count_ = count;
    mask_ = count - 1;
    age_ = 0;
}

void TranspositionTable::clear() {
    for (std::size_t i = 0; i < count_; ++i) entries_[i] = TTEntry{};
    age_ = 0;
}

bool TranspositionTable::probe(std::uint64_t key, TTEntry& out) const {
    const TTEntry& e = entries_[key & mask_];
    if (e.bound == BOUND_NONE || e.key != key) return false;
    out = e;
    return true;
}

void TranspositionTable::store(std::uint64_t key, Move move, int score, int depth, Bound bound) {
    TTEntry& e = entries_[key & mask_];
    bool occupied = e.bound != BOUND_NONE;
    bool same = occupied && e.key == key;

    bool overwrite;
    if (!occupied) {
        overwrite = true;
    } else if (same) {
        overwrite = depth >= e.depth || bound == BOUND_EXACT;
    } else {
        overwrite = e.age != age_ || depth >= e.depth;
    }

    if (!overwrite) {
        if (same && e.move.data == 0 && move.data != 0) e.move = move;
        return;
    }

    if (same && move.data == 0) move = e.move;

    e.key = key;
    e.move = move;
    e.score = static_cast<std::int16_t>(score);
    e.depth = static_cast<std::uint8_t>(depth);
    e.bound = bound;
    e.age = age_;
}

int TranspositionTable::hashfull() const {
    std::size_t sample = count_ < 1000 ? count_ : 1000;
    int used = 0;
    for (std::size_t i = 0; i < sample; ++i) {
        if (entries_[i].bound != BOUND_NONE && entries_[i].age == age_) ++used;
    }
    return static_cast<int>(used * 1000 / sample);
}

int score_to_tt(int score, int ply) {
    if (score > MATE_BOUND) return score + ply;
    if (score < -MATE_BOUND) return score - ply;
    return score;
}

int score_from_tt(int score, int ply) {
    if (score > MATE_BOUND) return score - ply;
    if (score < -MATE_BOUND) return score + ply;
    return score;
}
