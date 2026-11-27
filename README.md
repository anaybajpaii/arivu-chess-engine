# Arivu

Arivu is a UCI chess engine written from scratch in C++20. The name is the Tamil word for knowledge. It is a learning project: every part (move generation, evaluation, search) is implemented from first principles, with tests and strength measurements kept alongside the code.

Status: the engine plays complete, legal games and is usable from any UCI GUI. It is still a young engine with a simple evaluation, and it is far weaker than Stockfish (see Strength). It has no published Elo rating yet.

Co-authored by Claude AI.

## Features

**Board and move generation**
- Bitboards with a mailbox mirror
- 16-bit move encoding
- Pseudo-legal move generation with a make/unmake legality filter
- Perft verified against python-chess

**Evaluation**
- Material plus piece-square tables

**Search**
- Negamax alpha-beta (fail-soft) with iterative deepening
- Mate scores adjusted by ply
- Soft and hard time limits
- Quiescence search with MVV-LVA ordering
- Zobrist hashing
- Repetition detection, including history from the UCI `position ... moves` command (the first repetition is scored as a draw)
- Transposition table with bound types, mate-score ply adjustment, TT move ordering and depth-preferred replacement with aging

**UCI**
- Works with Cute Chess and lichess-bot
- Options: `Hash` (size in MB) and `Clear Hash`

**Not implemented yet**
- Null-move pruning, late move reductions, killer and history move ordering
- Opening book, endgame tablebases
- Multithreading, pondering

## Build

Requires a C++20 compiler. Tested with g++ 16.1.0 (MSYS2) on Windows 11.

```
g++ -std=c++20 -O2 -Wall -Wextra -Isrc src/main.cpp src/perft.cpp src/uci.cpp src/search.cpp src/eval.cpp src/board.cpp src/attacks.cpp src/movegen.cpp src/tt.cpp -o arivu
```

On Linux or macOS the same command works. The binary is then `./arivu`.

To use Arivu in a GUI, add `arivu.exe` as a UCI engine. For Cute Chess: Tools, Settings, Engines, Add, then set the command to the path of the executable and the protocol to UCI.

## Tests

The tests have their own entry point and are built from the same engine sources:

```
g++ -std=c++20 -O2 -Wall -Wextra -Isrc tests/tests.cpp src/perft.cpp src/uci.cpp src/search.cpp src/eval.cpp src/board.cpp src/attacks.cpp src/movegen.cpp src/tt.cpp -o arivu_tests
.\arivu_tests.exe
```

About 208 checks run in roughly 20 seconds. They include:
- perft counts compared against python-chess
- mutation testing of the test suite
- alpha-beta compared with a naive minimax for equal results
- transposition table on versus off, as a differential test
- runs under ASan and UBSan

Python tools are tested separately: `tools/test_elo.py` and `tools/test_match.py`.

## Repository layout

```
src/        engine sources
tests/      test suite (separate entry point)
tools/      Python match runner, Elo calculation, openings
docs/       Lichess setup and measurement method
LOG.md      running log of what was tried, what failed and what was learned
```

## Roadmap

- A distinctive element, chosen after the basics work
- A one-page technical writeup with results and plots

## License

MIT. See [LICENSE](LICENSE).
