#pragma once

#include <array>
#include <cstdint>

#include "types.h"

namespace zobrist {

struct Keys {
    std::array<std::array<std::uint64_t, 64>, PIECE_COUNT> piece{};
    std::array<std::uint64_t, 16> castling{};
    std::array<std::uint64_t, 8> ep_file{};
    std::uint64_t side = 0;
};

constexpr std::uint64_t next_random(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

constexpr Keys make_keys() {
    Keys keys;
    std::uint64_t state = 0x41524956555F3131ULL;
    for (auto& per_piece : keys.piece) {
        for (auto& key : per_piece) key = next_random(state);
    }
    for (auto& key : keys.castling) key = next_random(state);
    for (auto& key : keys.ep_file) key = next_random(state);
    keys.side = next_random(state);
    return keys;
}

inline constexpr Keys KEYS = make_keys();

}  // namespace zobrist