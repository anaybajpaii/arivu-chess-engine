# Measuring Arivu's strength

The scripts in `tools/` play matches between two UCI engines and report the score, the Elo difference and a 95% interval. They need Python 3.10+ and `python-chess`:

```powershell
py -m pip install -r tools\requirements.txt
```

## Running a match

```powershell
py tools\match.py --a .\arivu.exe --b .\old\arivu_commit12.exe --a-name Arivu --b-name Arivu-c12 --games 100 --movetime 0.1 --pgn results\self_play.pgn --json results\self_play.json
```

- `--a` is the engine being measured and `--b` is the opponent. The Elo difference is always A relative to B.
- Games are played in pairs: each opening is played twice with the colours swapped. There are 24 fixed openings of 8 plies in `tools/openings.py`, so every run is reproducible.
- `--movetime 0.1` gives each move a fixed 0.1 seconds. Use `--tc 60+0.6` instead for real clocks (base seconds plus increment). The harness takes time off each side's clock, adds the increment and records a loss on time.
- A game that ends with an illegal move, a crash or a flag is counted as a loss for that side and listed in a warning at the end. Fix the cause before trusting the numbers.
- Every game is saved to the PGN, with the opening and how it ended in the headers. Open it in Cute Chess or any PGN viewer.

## How many games

For two engines of similar strength, the 95% interval is about:

| games | interval |
|------:|---------:|
| 30 | ±107 Elo |
| 60 | ±75 Elo |
| 100 | ±57 Elo |
| 200 | ±40 Elo |
| 400 | ±29 Elo |

Below 30 games the script prints a warning because the interval is not reliable. A result that is far from even (say 90%) has a wider interval than this table, and a shutout is reported only as a bound.

## Playing against engines of known strength

Self-play only says whether a change helped. To place Arivu on a rating scale, play it against an engine whose strength is set. Stockfish can limit itself with `UCI_LimitStrength` and `UCI_Elo` (1320 to 3190 in the build I tested). Download Stockfish from https://stockfishchess.org/download/ and run:

```powershell
py tools\match.py --a .\arivu.exe --b C:\path\to\stockfish.exe --b-name "Stockfish 1800" --b-opt UCI_LimitStrength=true --b-opt UCI_Elo=1800 --b-opt Threads=1 --b-opt Hash=16 --games 60 --tc 60+0.6 --b-elo 1800 --pgn results\sf1800.pgn --json results\sf1800.json
```

Play several levels (for example 1500, 1800, 2100, 2400) so that you can bracket Arivu: the level where it scores about 50% is its approximate strength on that scale. `--b-elo` makes the script print an estimate, but treat it as indicative:

- Stockfish's `UCI_Elo` is calibrated by its developers against a specific rating list at a specific time control. Check the Stockfish wiki for the current wording before quoting it, and use a time control close to the calibration one.
- It is not the same scale as a Lichess rating.
- Engines that limit strength by playing deliberate mistakes do not behave like an engine of that rating at a fast time control.

## Rules that keep the numbers honest

- One game at a time per CPU core at most. Use `--concurrency` no higher than your core count minus one, and 1 for anything that matters. If games compete for the CPU, both engines get less time than the clock says.
- Do nothing else heavy on the computer while a match runs.
- Both engines single-threaded, with the same hash size where possible.
- Decide the number of games before you start and report all of them. Do not stop when the result looks good.
- Report the interval with the Elo, the time control, the number of games and the Arivu commit.
- Keep the PGN and JSON files in `results/` and commit them.

## Reading the output

```
Arivu vs Arivu-c12: 100 games  +46 =30 -24
score 61.0%
Elo difference +78  (95% interval +21 to +138)
likelihood that Arivu is stronger 99.6%
```

`likelihood` is the chance that A is really the stronger engine given only the wins and losses. It is not a promise that a rating gap is exactly the printed number.

## Tests

```powershell
cd tools
py -m unittest test_elo test_match
```
