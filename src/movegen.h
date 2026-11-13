#pragma once

#include "board.h"
#include "move.h"

bool is_square_attacked(const Board& board, Square s, Color by);
bool in_check(const Board& board, Color side);
void generate_pseudo_legal(const Board& board, MoveList& list);