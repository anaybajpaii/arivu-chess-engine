#pragma once

#include <cstdint>

#include "board.h"

std::uint64_t perft(Board& board, int depth);
std::uint64_t divide(Board& board, int depth);