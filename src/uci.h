#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "board.h"

bool parse_position(Board& board, const std::string& args, std::vector<std::uint64_t>& history);
void uci_loop(std::istream& in, std::ostream& out);