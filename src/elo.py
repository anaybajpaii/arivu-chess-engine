import argparse
import math
from dataclasses import dataclass


MIN_RELIABLE_GAMES = 30


@dataclass(frozen=True)
class Summary:
    wins: int
    draws: int
    losses: int
    games: int
    score: float
    elo: float
    elo_low: float
    elo_high: float
    los: float
    bounded: bool


def elo_from_score(score):
    return -400.0 * math.log10(1.0 / score - 1.0)


def score_from_elo(elo):
    return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))


def likelihood_of_superiority(wins, losses):
    if wins + losses == 0:
        return 0.5
    return 0.5 * (1.0 + math.erf((wins - losses) / math.sqrt(2.0 * (wins + losses))))


def summarize(wins, draws, losses, z=1.96):
    games = wins + draws + losses
    if games <= 0:
        raise ValueError("at least one game is needed")
    if min(wins, draws, losses) < 0:
        raise ValueError("results cannot be negative")

    raw_score = (wins + 0.5 * draws) / games
    bounded = raw_score <= 0.0 or raw_score >= 1.0
    floor = 1.0 / (2.0 * games)
    score = min(max(raw_score, floor), 1.0 - floor)

    mean_square = (wins * (1.0 - score) ** 2 + draws * (0.5 - score) ** 2 + losses * score**2) / games
    standard_error = math.sqrt(mean_square / games)
    low = min(max(score - z * standard_error, floor), 1.0 - floor)
    high = min(max(score + z * standard_error, floor), 1.0 - floor)

    return Summary(
        wins=wins,
        draws=draws,
        losses=losses,
        games=games,
        score=raw_score,
        elo=elo_from_score(score),
        elo_low=elo_from_score(low),
        elo_high=elo_from_score(high),
        los=likelihood_of_superiority(wins, losses),
        bounded=bounded,
    )


def format_summary(summary, name_a="A", name_b="B"):
    lines = [
        f"{name_a} vs {name_b}: {summary.games} games  +{summary.wins} ={summary.draws} -{summary.losses}",
        f"score {summary.score * 100:.1f}%",
    ]
    qualifier = ""
    if summary.bounded:
        qualifier = " (all games went one way, so this is only a bound)"
    lines.append(
        f"Elo difference {summary.elo:+.0f}  (95% interval {summary.elo_low:+.0f} to {summary.elo_high:+.0f}){qualifier}"
    )
    lines.append(f"likelihood that {name_a} is stronger {summary.los * 100:.1f}%")
    if summary.games < MIN_RELIABLE_GAMES:
        lines.append(
            f"warning: only {summary.games} games, so the interval is unreliable "
            f"(aim for at least {MIN_RELIABLE_GAMES}, ideally 100 or more)"
        )
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Elo difference with an error bar from win/draw/loss counts")
    parser.add_argument("--wins", type=int, required=True)
    parser.add_argument("--draws", type=int, required=True)
    parser.add_argument("--losses", type=int, required=True)
    args = parser.parse_args()
    print(format_summary(summarize(args.wins, args.draws, args.losses)))


if __name__ == "__main__":
    main()
