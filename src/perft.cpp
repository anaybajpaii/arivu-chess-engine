#include "perft.h"

#include <iostream>

#include "movegen.h"

std::uint64_t perft(Board& board, int depth) {
    if (depth <= 0) return 1;

    MoveList list;
    generate_legal(board, list);
    if (depth == 1) return static_cast<std::uint64_t>(list.count);

    std::uint64_t nodes = 0;
    for (const Move& m : list) {
        Undo u = board.make_move(m);
        nodes += perft(board, depth - 1);
        board.unmake_move(m, u);
    }
    return nodes;
}

std::uint64_t divide(Board& board, int depth) {
    MoveList list;
    generate_legal(board, list);

    std::uint64_t total = 0;
    for (const Move& m : list) {
        Undo u = board.make_move(m);
        std::uint64_t nodes = perft(board, depth - 1);
        board.unmake_move(m, u);
        std::cout << m.to_uci() << ": " << nodes << "\n";
        total += nodes;
    }
    std::cout << "\nmoves: " << list.count << "\nnodes: " << total << "\n";
    return total;
}