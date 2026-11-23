#pragma once

#include <iosfwd>
#include <string>

#include "board.h"

bool parse_position(Board& board, const std::string& args);
void uci_loop(std::istream& in, std::ostream& out);