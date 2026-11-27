# Arivu development log

A running record of what was tried, what failed and what was learned. Newest entries at the bottom of each section. Numbers are from my own test runs on the stated settings, not from a rating list.

## Rules for the project

- Own implementation. No code copied from Stockfish or any other engine.
- Roughly 2 hours a week from Nov 22. Checkpoint Dec 13: the engine must have played legal games on Lichess with a measured rating, otherwise the repo is dropped from the portfolio and kept as a personal project. Freeze Dec 28.
- Strength is measured honestly: controlled matches with Elo and error bars, not impressions.

## Timeline

### Setup (Nov 12 to Nov 23)

- Nov 12: decided that Claude writes the code commit by commit and explains it, with the help credited.
- Nov 15 to 16: agreed the 15-commit plan (types and bitboards, attacks and move generation, make/unmake, perft and tests, UCI, evaluation, search, quiescence, Zobrist, transposition table, Lichess bridge and measurement, a distinctive element, writeup). Started commit 5.
- Nov 20: tests live in `tests/` with their own entry point (`arivu_tests`); engine code stays in `src/`.
- By Nov 23: commits 5 and 6 (make/unmake with legality filtering, perft and the test suite) done.

### Commits 5 and 6: make/unmake and perft

- Tried: perft node counts for six positions as the correctness check for move generation.
- Failed: reference node counts for two of the positions were quoted from memory and were wrong. The engine was right and the numbers were not.
- Learned: take reference numbers from an independent program (python-chess), never from memory.

### Commits 7 to 9: UCI, evaluation, search (Nov 23 to 24)

- Tried: material and piece-square evaluation, negamax with alpha-beta, iterative deepening, time limits, mate scores.
- Failed: a mutation test (removing the hard time cap) was not caught by the suite. Added a test that Kiwipete answers `go movetime 100` in under 400 ms.
- Learned: alpha-beta must give exactly the same score as plain minimax at a given depth, so the suite compares the two on six positions. Mirror symmetry of the evaluation is a cheap, strong check.
- Tooling problem: `cutechess-cli` was not available (only the Cute Chess GUI is installed). Games were played in the GUI, and matches are now run by `tools/match.py`.

### Commit 10: quiescence search and MVV-LVA ordering (Nov 24)

- Result: start position to depth 6 went from about 5.6 s to about 0.5 s, and Kiwipete to depth 6 from about 16 s to under 1 s. The horizon effect (taking a defended pawn at the end of the search) is fixed and tested.
- Result: 27 wins, 1 loss, 2 draws against the commit 9 engine at 0.1 s per move.
- Failed: the first version of the test reference used a fully unpruned quiescence search. It never finished: even depth 1 on Kiwipete did not return. Replaced by a reference whose quiescence is a plain alpha-beta in natural move order.
- Learned: without move ordering quiescence search explodes on tactical positions, which is exactly what MVV-LVA fixes. Also, one mutation (making promotions non-noisy) survived until a test with a pawn about to promote was added.

### Commit 11: Zobrist hashing and repetition detection (Nov 24)

- Result: incremental hash verified against a full recompute at every position to depth 3 on six positions and in 30 random games. About 8% fewer nodes per second from the hash upkeep. A real forced perpetual check (verified with python-chess) is now scored as a draw.
- Result: 13 wins, 10 losses, 17 draws against commit 10 at 0.1 s per move. That is within noise, so no strength gain is claimed. The commit is correctness and groundwork for the transposition table.
- Learned: a repeat is scored as a draw on the first repetition, including positions from the real game. This is common engine practice and is a deliberate simplification.

### Commit 12: transposition table (Nov 25)

- Result: start position to depth 7 went from 18.7M nodes (6.4 s) to 1.58M nodes (0.63 s) with the same score. Mate-in-4 and mated-in-3 searches to depth 8 went from about 5M nodes to about 100K.
- Result: 38 wins, 16 losses, 6 draws against commit 11 at 0.1 s per move: about +134 Elo, 95% interval roughly +51 to +234 (60 games).
- Verified: the same score with and without the table on more than 400 positions to depth 8. Restricting reuse to exact depth made every score identical, so bounds and mate distances are right. Normal mode differs by 1 to 3 centipawns in a few long endgames because it reuses deeper results; that is expected.
- Failed: after the first set of tests, 7 deliberately broken versions of the bounds and cutoff logic still passed. They only change a score in about 1 position in 40, so shallow tests never hit them. Added ten positions where the broken versions diverge. All now caught.
- Learned: a test suite can pass while missing a whole class of bug. Breaking the code on purpose and checking that the tests notice is the only way to find that out.

### Commit 13: Lichess bridge and measurement (Nov 25)

- Built `tools/match.py` and `tools/elo.py` (match runner, Elo with 95% interval, PGN and JSON output), with unit tests.
- Checked the harness against Stockfish 19 Lite with `UCI_Elo` set to 1320, 1800 and 2200 (8 games each, 0.1 s per move, four games at once on a two-core machine). Arivu scored about 94%, 75% and 50%. This only shows the harness works and Arivu is at least club strength; the timing was distorted and the sample is tiny, so no rating is claimed from it.
- Observed: Arivu is fully deterministic, so it plays the same moves and the same games every time from the same position.
- Not done yet: the Lichess BOT upgrade, the first games on Lichess and the real gauntlet runs.

## Measurements

| Date | What | Settings | Result |
|---|---|---|---|
| Nov 24 | Commit 10 vs commit 9 | 0.1 s/move, random 6-ply openings | +27 =2 -1 (30 games) |
| Nov 24 | Commit 11 vs commit 10 | 0.1 s/move, random 6-ply openings | +13 =17 -10 (40 games) |
| Nov 25 | Commit 12 vs commit 11 | 0.1 s/move, random 6-ply openings | +38 =6 -16 (60 games), about +134 Elo (+51 to +234) |

Lichess rating (fill in after the first rated games): not yet measured.

## Known limitations

- Single-threaded, one fixed search style, no opening book, no endgame tablebases.
- Deterministic play.
- Repetition is scored as a draw at the first repeat; the transposition table can carry a repetition draw into a score reached by another route.
- Quiescence search does not use the transposition table or check for repetition.