#include "search.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ostream>
#include <string>
#include <utility>

#include "eval.h"
#include "movegen.h"

namespace {

constexpr int MATE_THRESHOLD = MATE_SCORE - 1000;
constexpr std::uint64_t TIME_CHECK_MASK = 2047;
constexpr int MAX_PLY = 128;
constexpr int CAPTURE_BASE = 10000;
constexpr int QUEEN_PROMOTION_BASE = 20000;
constexpr int PREVIOUS_BEST_BONUS = 1000000;

using ScoreList = std::array<int, 256>;

std::string score_string(int score) {
    if (score > MATE_THRESHOLD) return "mate " + std::to_string((MATE_SCORE - score + 1) / 2);
    if (score < -MATE_THRESHOLD) return "mate -" + std::to_string((MATE_SCORE + score) / 2);
    return "cp " + std::to_string(score);
}

bool is_noisy(Move m) {
    return m.is_capture() || (m.is_promotion() && m.promotion_type() == QUEEN);
}

int order_score(const Board& b, Move m) {
    int score = 0;
    if (m.is_capture()) {
        PieceType victim = (m.flags() == EP_CAPTURE) ? PAWN : type_of(b.mailbox[m.to()]);
        PieceType attacker = type_of(b.mailbox[m.from()]);
        score += CAPTURE_BASE + 10 * victim - attacker;
    }
    if (m.is_promotion() && m.promotion_type() == QUEEN) score += QUEEN_PROMOTION_BASE;
    return score;
}

void score_moves(const Board& b, const MoveList& list, ScoreList& scores) {
    for (int i = 0; i < list.count; ++i) scores[i] = order_score(b, list.moves[i]);
}

void pick_best(MoveList& list, ScoreList& scores, int from) {
    int best = from;
    for (int i = from + 1; i < list.count; ++i) {
        if (scores[i] > scores[best]) best = i;
    }
    if (best != from) {
        std::swap(list.moves[from], list.moves[best]);
        std::swap(scores[from], scores[best]);
    }
}

class Searcher {
public:
    Searcher(Board& board, const SearchLimits& limits)
        : board_(board), limits_(limits), start_(Clock::now()) {}

    SearchResult run(std::ostream* info);

private:
    using Clock = std::chrono::steady_clock;

    long long elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count();
    }

    void check_time() {
        if (can_stop_ && limits_.hard_ms > 0 && elapsed_ms() >= limits_.hard_ms) stopped_ = true;
    }

    int negamax(int depth, int alpha, int beta, int ply);
    int quiescence(int alpha, int beta, int ply);

    Board& board_;
    SearchLimits limits_;
    Clock::time_point start_;
    std::uint64_t nodes_ = 0;
    bool stopped_ = false;
    bool can_stop_ = false;
};

int Searcher::quiescence(int alpha, int beta, int ply) {
    ++nodes_;
    if ((nodes_ & TIME_CHECK_MASK) == 0) check_time();
    if (stopped_) return 0;
    if (ply >= MAX_PLY) return evaluate(board_);

    Color us = board_.side_to_move;
    bool checked = in_check(board_, us);

    MoveList list;
    if (checked) {
        generate_legal(board_, list);
        if (list.count == 0) return -MATE_SCORE + ply;
        if (board_.halfmove_clock >= 100) return 0;
    } else {
        if (board_.halfmove_clock >= 100) return 0;
        generate_pseudo_legal(board_, list);
    }

    int best = -INF_SCORE;
    if (!checked) {
        int stand_pat = evaluate(board_);
        if (stand_pat >= beta) return stand_pat;
        best = stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;
    }

    ScoreList scores;
    score_moves(board_, list, scores);

    for (int i = 0; i < list.count; ++i) {
        pick_best(list, scores, i);
        Move m = list.moves[i];
        if (!checked && !is_noisy(m)) break;

        Undo u = board_.make_move(m);
        if (in_check(board_, us)) {
            board_.unmake_move(m, u);
            continue;
        }
        int score = -quiescence(-beta, -alpha, ply + 1);
        board_.unmake_move(m, u);
        if (stopped_) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) alpha = score;
        }
        if (alpha >= beta) break;
    }
    return best;
}

int Searcher::negamax(int depth, int alpha, int beta, int ply) {
    if (depth <= 0) return quiescence(alpha, beta, ply);

    ++nodes_;
    if ((nodes_ & TIME_CHECK_MASK) == 0) check_time();
    if (stopped_) return 0;

    MoveList list;
    generate_legal(board_, list);
    if (list.count == 0) return in_check(board_, board_.side_to_move) ? -MATE_SCORE + ply : 0;
    if (board_.halfmove_clock >= 100) return 0;

    ScoreList scores;
    score_moves(board_, list, scores);

    int best = -INF_SCORE;
    for (int i = 0; i < list.count; ++i) {
        pick_best(list, scores, i);
        Move m = list.moves[i];
        Undo u = board_.make_move(m);
        int score = -negamax(depth - 1, -beta, -alpha, ply + 1);
        board_.unmake_move(m, u);
        if (stopped_) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) alpha = score;
        }
        if (alpha >= beta) break;
    }
    return best;
}

SearchResult Searcher::run(std::ostream* info) {
    SearchResult result;

    MoveList root;
    generate_legal(board_, root);
    if (root.count == 0) {
        result.score = in_check(board_, board_.side_to_move) ? -MATE_SCORE : 0;
        return result;
    }

    result.found = true;
    result.best = root.moves[0];
    if (root.count == 1) return result;

    ScoreList root_scores;

    for (int depth = 1; depth <= limits_.max_depth; ++depth) {
        score_moves(board_, root, root_scores);
        for (int i = 0; i < root.count; ++i) {
            if (root.moves[i] == result.best) root_scores[i] += PREVIOUS_BEST_BONUS;
        }

        int alpha = -INF_SCORE;
        int iteration_score = -INF_SCORE;
        Move iteration_best = result.best;

        for (int i = 0; i < root.count; ++i) {
            pick_best(root, root_scores, i);
            Move m = root.moves[i];
            Undo u = board_.make_move(m);
            int score = -negamax(depth - 1, -INF_SCORE, -alpha, 1);
            board_.unmake_move(m, u);
            if (stopped_) break;

            if (score > iteration_score) {
                iteration_score = score;
                iteration_best = m;
                if (score > alpha) alpha = score;
            }
        }
        if (stopped_) break;

        result.best = iteration_best;
        result.score = iteration_score;
        result.depth = depth;
        can_stop_ = true;

        if (info) {
            long long ms = elapsed_ms();
            std::uint64_t nps = nodes_ * 1000 / static_cast<std::uint64_t>(std::max(1LL, ms));
            *info << "info depth " << depth << " score " << score_string(iteration_score)
                  << " nodes " << nodes_ << " nps " << nps << " time " << ms << " pv "
                  << iteration_best.to_uci() << "\n"
                  << std::flush;
        }

        if (iteration_score > MATE_THRESHOLD) break;
        if (limits_.soft_ms > 0 && elapsed_ms() >= limits_.soft_ms) break;
    }

    result.nodes = nodes_;
    return result;
}

}  // namespace

SearchResult search(Board& board, const SearchLimits& limits, std::ostream* info) {
    Searcher searcher(board, limits);
    return searcher.run(info);
}