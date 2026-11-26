# Running Arivu on Lichess

This puts Arivu on Lichess as a bot account using lichess-bot, the official bridge between Lichess and UCI engines. Every move in every game comes from `arivu.exe`.

These steps follow the lichess-bot wiki and the Lichess API documentation as read on 2026-11-25. Both change over time, so if a step does not match what you see, trust the wiki and the file comments in `config.yml.default`.

- lichess-bot: https://github.com/lichess-bot-devs/lichess-bot (wiki: How to Install, Configure lichess-bot, Setup the engine, Upgrade to a BOT account, How to Run)
- Lichess bot rules: https://lichess.org/blog/WvDNticAAMu_mHKP/welcome-lichess-bots

## 0. Decide which account becomes the bot (do this before anything else)

Upgrading an account to a BOT account is irreversible. Lichess also says an upgraded account cannot play through the website afterwards, and the lichess-bot wiki says an account that has already played games cannot be used as a bot.

- If the account you have now is meant to be your personal account, create a second account for the bot and use that one for everything below.
- If it is a fresh account you made for Arivu and it has played no games, you can use it.

Do not continue past step 5 until you are sure.

## 1. Install lichess-bot

You need Python 3.10 or newer. Keep lichess-bot in its own folder next to the Arivu repo, not inside it.

```powershell
cd C:\path\to\lichess-bot
py -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

If PowerShell refuses to run `activate`, run this once and try again:

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

## 2. Put Arivu in place

Build the engine in the Arivu repo with the usual line (it needs `src/tt.cpp`), then copy it:

```powershell
cd C:\path\to\arivu-chess-engine
g++ -std=c++20 -O2 -Wall -Wextra -Isrc src/main.cpp src/perft.cpp src/uci.cpp src/search.cpp src/eval.cpp src/board.cpp src/attacks.cpp src/movegen.cpp src/tt.cpp -o arivu
mkdir C:\path\to\lichess-bot\engines -ErrorAction SilentlyContinue
copy arivu.exe C:\path\to\lichess-bot\engines\arivu.exe
```

## 3. Create config.yml

```powershell
cd C:\path\to\lichess-bot
copy config.yml.default config.yml
```

Open `config.yml` and change only the following. Keep the file's own layout and indentation, and keep every other line as it is.

**Engine**

```yaml
engine:
  dir: "./engines/"
  name: "arivu.exe"
  protocol: "uci"
  ponder: false
```

- The `engine` block has an options section (called `uci_options` or `engine_options` depending on the version). lichess-bot stops with an error if it names an option the engine does not declare. Arivu declares only `Hash` and `Clear Hash`, so delete every entry in that section, or leave only `Hash: 64`.
- Do not add `Threads`, `Move Overhead`, `SyzygyPath` or similar. Arivu is single-threaded and has no tablebase support.

**Every move must come from Arivu.** Make sure each of these is switched off, because otherwise moves can come from an opening book, the internet or tablebases and the measured rating would not be Arivu's:

- `polyglot`: `enabled: false`
- `online_moves`: `enabled: false` in each of `chessdb_book`, `lichess_cloud_analysis`, `lichess_opening_explorer` and `online_egtb`
- endgame tablebases (Syzygy / Gaviota), if the file has them: `enabled: false`
- `draw_or_resign`: `resign_enabled: false` and `offer_draw_enabled: false`

**Time safety**

```yaml
move_overhead: 300
```

This is in milliseconds and is subtracted from each move's thinking time to cover network delay. Raise it if the log ever shows a game lost on time.

**Challenges.** Start with one game at a time and ordinary time controls:

```yaml
challenge:
  concurrency: 1
  variants:
    - standard
  time_controls:
    - blitz
    - rapid
```

For `modes`, start with casual only. Add rated once the first casual games have gone cleanly (step 8). If you want the bot to find opponents by itself, set `allow_matchmaking: true` in the matchmaking section and use `challenge_initial_time: 180`, `challenge_increment: 2`, `challenge_variant: "standard"`, `challenge_mode: "casual"`.

## 4. Create the token

1. Log in as the account that will be the bot.
2. Open https://lichess.org/account/oauth/token/create?scopes%5B%5D=bot:play&description=lichess-bot
3. Check that "Play games with the bot API" (`bot:play`) is ticked, create the token and copy it. Lichess shows it once.
4. Store it in the `token:` field of `config.yml`, or set the `LICHESS_BOT_TOKEN` environment variable.

The token is a password for your account. Never paste it into a chat, a screenshot, a commit or the Arivu repo. If it leaks, delete it on Lichess and make a new one.

## 5. Upgrade to a BOT account (irreversible)

Only after step 0. With the virtual environment active, in the lichess-bot folder:

```powershell
python lichess-bot.py -u
```

This upgrades the account whose token is in the config. It cannot be undone, and the account can no longer play from the website. Lichess may also reject the upgrade if the account has already played a game.

## 6. Run the bot

```powershell
python lichess-bot.py -v --logfile bot.log
```

`-v` prints the engine's thinking output into the log, which is useful as evidence that the moves come from Arivu. Stop it with `Ctrl+C`. Do not close the window while a game is in progress, because the game would be lost on time.

## 7. Challenge it

From another Lichess account, open the bot's profile and press "Challenge to a game". Use casual, 3+2 or 5+3, standard. Or enable matchmaking in step 3 and let it challenge other bots.

## 8. Checks before rated games

Play three to five casual games and confirm all of the following. Stop and fix any that fail:

- The bot accepts challenges and answers every move.
- No game ends on time. Check the clock at the end of each game.
- No game ends with an illegal move or an engine error in `bot.log`.
- The log shows moves coming from `arivu.exe` (a line with the engine's `info depth` output for each move).

Then change `modes` to include rated and keep going.

## 9. Record the measured rating

Lichess marks a rating as provisional with a `?`. Keep the bot playing rated games until the `?` goes away, then record all of the following in `LOG.md`:

- the rating shown for each time control (for example Blitz), the date and the number of rated games
- the time controls played and, roughly, the strength of the opponents
- a link to the bot's profile and a screenshot of the rating
- the Arivu commit the bot was running

Be careful how this number is described. A bot's Lichess rating comes from games against other bots and some humans in Lichess's own rating pool. It is not comparable with a FIDE or CCRL rating, or with Stockfish's `UCI_Elo` scale. Write "Lichess blitz rating of N against bots" and not "Elo N". The controlled matches in `docs/MEASURING.md` are the better evidence of strength, and the Lichess rating is the evidence that it plays real games.

## Troubleshooting

- **The bot exits at startup with an unknown option error:** an option listed under the engine's options section is not one Arivu declares. Remove it.
- **"engine not found":** `dir` and `name` do not match where `arivu.exe` is. Check `engines\arivu.exe` exists and `name` includes `.exe`.
- **Games are lost on time:** raise `move_overhead`, and prefer 3+2 or slower for now.
- **No challenges arrive:** nothing is sent automatically unless you turn on matchmaking or challenge it yourself.
- **The upgrade is refused:** read the error message. An account that has already played games cannot be used, so make a new account.
