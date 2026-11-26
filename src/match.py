import argparse
import concurrent.futures
import datetime
import json
import os
import sys
import threading
import time

import chess
import chess.engine
import chess.pgn

import elo
from openings import OPENINGS

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_option_value(text):
    lowered = text.lower()
    if lowered in ("true", "false"):
        return lowered == "true"
    try:
        return int(text)
    except ValueError:
        return text


def parse_options(pairs):
    options = {}
    for pair in pairs or []:
        if "=" not in pair:
            raise ValueError(f"option must look like Name=Value, got {pair!r}")
        name, value = pair.split("=", 1)
        options[name.strip()] = parse_option_value(value.strip())
    return options


def parse_time_control(text):
    base, _, increment = text.partition("+")
    return float(base), float(increment or 0.0)


def opening_for_game(index, opening_count):
    return (index // 2) % opening_count


def opening_board(line):
    board = chess.Board()
    for san in line.split():
        board.push_san(san)
    return board


class EngineSpec:
    def __init__(self, name, path, args, options):
        self.name = name
        self.command = [path] + list(args)
        self.options = options

    def start(self):
        engine = chess.engine.SimpleEngine.popen_uci(self.command, timeout=60)
        if self.options:
            engine.configure(self.options)
        return engine


class Worker:
    def __init__(self, spec_a, spec_b):
        self.engines = {"A": spec_a.start(), "B": spec_b.start()}

    def close(self):
        for engine in self.engines.values():
            try:
                engine.quit()
            except Exception:
                engine.close()


def play_game(worker, spec_a, spec_b, index, args):
    opening_index = opening_for_game(index, len(OPENINGS))
    opening_name, line = OPENINGS[opening_index]
    a_is_white = index % 2 == 0
    board = opening_board(line)
    start_ply = len(board.move_stack)

    game_token = object()
    clocks = {chess.WHITE: 0.0, chess.BLACK: 0.0}
    increment = 0.0
    if args.tc:
        base, increment = parse_time_control(args.tc)
        clocks = {chess.WHITE: base, chess.BLACK: base}

    result = None
    termination = None
    winner_color = None

    while True:
        outcome = board.outcome(claim_draw=True)
        if outcome is not None:
            termination = outcome.termination.name.lower()
            winner_color = outcome.winner
            result = "1/2-1/2" if outcome.winner is None else ("1-0" if outcome.winner == chess.WHITE else "0-1")
            break
        if len(board.move_stack) >= args.max_plies:
            termination = "ply limit"
            result = "1/2-1/2"
            break

        side = board.turn
        mover = "A" if (side == chess.WHITE) == a_is_white else "B"
        engine = worker.engines[mover]

        if args.tc:
            limit = chess.engine.Limit(
                white_clock=clocks[chess.WHITE],
                black_clock=clocks[chess.BLACK],
                white_inc=increment,
                black_inc=increment,
            )
        else:
            limit = chess.engine.Limit(time=args.movetime)

        started = time.monotonic()
        try:
            reply = engine.play(board, limit, game=game_token)
            move = reply.move
        except Exception as error:
            termination = f"{mover} failed: {type(error).__name__}"
            result = "0-1" if side == chess.WHITE else "1-0"
            winner_color = not side
            break
        elapsed = time.monotonic() - started

        if move is None or move not in board.legal_moves:
            termination = f"{mover} played an illegal move"
            result = "0-1" if side == chess.WHITE else "1-0"
            winner_color = not side
            break

        if args.tc:
            clocks[side] -= elapsed
            if clocks[side] <= 0:
                termination = f"{mover} lost on time"
                result = "0-1" if side == chess.WHITE else "1-0"
                winner_color = not side
                break
            clocks[side] += increment

        board.push(move)

    game = chess.pgn.Game()
    game.headers["Event"] = "Arivu match"
    game.headers["Site"] = "local"
    game.headers["Date"] = datetime.date.today().strftime("%Y.%m.%d")
    game.headers["Round"] = str(index + 1)
    game.headers["White"] = spec_a.name if a_is_white else spec_b.name
    game.headers["Black"] = spec_b.name if a_is_white else spec_a.name
    game.headers["Result"] = result
    game.headers["Opening"] = opening_name
    game.headers["TimeControl"] = args.tc if args.tc else f"{args.movetime}s/move"
    game.headers["Termination"] = termination
    node = game
    for move in board.move_stack:
        node = node.add_variation(move)
    if start_ply:
        game.comment = f"First {start_ply} plies from the opening list ({opening_name})."

    if winner_color is None:
        a_result = "draw"
    elif (winner_color == chess.WHITE) == a_is_white:
        a_result = "win"
    else:
        a_result = "loss"

    return {
        "index": index,
        "a_is_white": a_is_white,
        "result": result,
        "a_result": a_result,
        "termination": termination,
        "plies": len(board.move_stack),
        "opening": opening_name,
        "pgn": str(game),
    }


def run_match(spec_a, spec_b, args):
    games = args.games
    outcomes = []
    lock = threading.Lock()
    local = threading.local()
    workers = []

    def task(index):
        if not hasattr(local, "worker"):
            local.worker = Worker(spec_a, spec_b)
            with lock:
                workers.append(local.worker)
        record = play_game(local.worker, spec_a, spec_b, index, args)
        with lock:
            outcomes.append(record)
            wins = sum(1 for r in outcomes if r["a_result"] == "win")
            draws = sum(1 for r in outcomes if r["a_result"] == "draw")
            losses = sum(1 for r in outcomes if r["a_result"] == "loss")
            colour = "white" if record["a_is_white"] else "black"
            print(
                f"game {record['index'] + 1:>3}/{games}  {spec_a.name} as {colour:<5}  "
                f"{record['a_result']:<4}  {record['termination']}  ({record['plies']} plies)   "
                f"running +{wins} ={draws} -{losses}",
                flush=True,
            )
        return record

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
            list(pool.map(task, range(games)))
    finally:
        for worker in workers:
            worker.close()

    outcomes.sort(key=lambda r: r["index"])
    return outcomes


def build_parser():
    parser = argparse.ArgumentParser(description="Play a match between two UCI engines and report Elo with an error bar")
    parser.add_argument("--a", required=True, help="path to engine A (the engine being measured)")
    parser.add_argument("--b", required=True, help="path to engine B (the opponent)")
    parser.add_argument("--a-name", default="Arivu")
    parser.add_argument("--b-name", default="Opponent")
    parser.add_argument("--a-args", nargs="*", default=[], help="extra command line arguments for engine A")
    parser.add_argument("--b-args", nargs="*", default=[], help="extra command line arguments for engine B")
    parser.add_argument("--a-opt", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--b-opt", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--games", type=int, default=40, help="number of games (use an even number)")
    parser.add_argument("--movetime", type=float, default=0.1, help="seconds per move when --tc is not given")
    parser.add_argument("--tc", help="time control as BASE+INCREMENT in seconds, for example 10+0.1")
    parser.add_argument("--concurrency", type=int, default=1, help="games played at the same time")
    parser.add_argument("--max-plies", type=int, default=400, help="games longer than this are drawn")
    parser.add_argument("--pgn", help="write all games to this PGN file")
    parser.add_argument("--json", help="write the summary to this JSON file")
    parser.add_argument("--b-elo", type=float, help="known rating of engine B, to print an estimated rating for A")
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.games < 1:
        sys.exit("--games must be at least 1")
    if args.games % 2:
        print("note: an odd number of games gives one colour an extra game", file=sys.stderr)

    spec_a = EngineSpec(args.a_name, args.a, args.a_args, parse_options(args.a_opt))
    spec_b = EngineSpec(args.b_name, args.b, args.b_args, parse_options(args.b_opt))

    outcomes = run_match(spec_a, spec_b, args)

    wins = sum(1 for r in outcomes if r["a_result"] == "win")
    draws = sum(1 for r in outcomes if r["a_result"] == "draw")
    losses = sum(1 for r in outcomes if r["a_result"] == "loss")
    summary = elo.summarize(wins, draws, losses)

    print()
    print(elo.format_summary(summary, spec_a.name, spec_b.name))
    if args.b_elo is not None:
        print(
            f"rating estimate for {spec_a.name}: {args.b_elo + summary.elo:.0f} "
            f"({args.b_elo + summary.elo_low:.0f} to {args.b_elo + summary.elo_high:.0f}) "
            f"if {spec_b.name} really is {args.b_elo:.0f} at this time control"
        )
    problems = [r for r in outcomes if "failed" in r["termination"] or "illegal" in r["termination"] or "on time" in r["termination"]]
    if problems:
        print(f"WARNING: {len(problems)} game(s) ended by a failure, illegal move or flag:")
        for record in problems:
            print(f"  game {record['index'] + 1}: {record['termination']}")

    if args.pgn:
        with open(args.pgn, "w", encoding="utf-8") as handle:
            for record in outcomes:
                handle.write(record["pgn"] + "\n\n")
        print(f"games written to {args.pgn}")
    if args.json:
        payload = {
            "a": spec_a.name,
            "b": spec_b.name,
            "wins": wins,
            "draws": draws,
            "losses": losses,
            "score": summary.score,
            "elo": summary.elo,
            "elo_low": summary.elo_low,
            "elo_high": summary.elo_high,
            "time_control": args.tc if args.tc else f"{args.movetime}s/move",
            "a_options": spec_a.options,
            "b_options": spec_b.options,
            "b_elo": args.b_elo,
            "failures": [r["termination"] for r in problems],
        }
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2)
        print(f"summary written to {args.json}")
    return 0 if not problems else 2


if __name__ == "__main__":
    sys.exit(main())
