// search.h
// Very simple move selection engine for Gomoku.

#ifndef GOMOKU_SEARCH_H
#define GOMOKU_SEARCH_H

#include "board.h"
#include "threat_solver.h"

#include <chrono>
#include <vector>
#include <unordered_map>

#include "history_heuristic.h"

namespace gomoku {

// A naive search engine that chooses a reasonable move.
// For demonstration purposes, this implementation selects the legal move
// closest to the center of the board.  In a real engine this class
// would implement a proper game‑tree search.
class SearchEngine {
public:
    SearchEngine();
    // Return the next move for the given board and player using an
    // iterative deepening alpha–beta search.  The search respects a time
    // limit in milliseconds (default 2000ms).  The board is passed by
    // non-const reference so that the engine can apply and unapply moves
    // during its search.
    Move findBestMove(Board &board, Player myColor, int timeLimitMs = 2000);

    // Determine whether an opening move applies to the current position.
    // If an opening move exists for the player myColor, this function
    // assigns it to outMove and returns true.  Otherwise it returns false.
    bool getOpeningMove(const Board &board, Player myColor, Move &outMove) const;

private:
    // Time management helpers.
    void startTimer(int timeLimitMs);
    bool timeUp() const;

    // Evaluation of the board from the perspective of myColor.
    int evaluate(const Board &board, Player myColor) const;
    int evaluatePlayer(const Board &board, Player player) const;
    int patternScore(int count, bool leftOpen, bool rightOpen) const;

    // Alpha–beta search.
    int alphaBeta(Board &board, int depth, int alpha, int beta,
                  Player currentPlayer, Player myColor, int ply);

    // Generate and sort candidate moves.  Sorting is based on a simple
    // heuristic that prioritizes moves that yield immediate wins or
    // block the opponent's winning opportunities.
    std::vector<Move> orderMoves(Board &board, Player currentPlayer, Player myColor, int ply);

    // Timer end point.
    std::chrono::steady_clock::time_point timeEnd;
    // Maximum search depth reached during current search (for reporting).
    int maxDepthReached;

    // Tactical threat detector used to surface urgent defensive moves, such as
    // blocking open threes or fours from the opponent.
    ThreatSolver threatSolver;

    // --- Transposition table ---
    // Each entry stores the best known score for a position at a given depth,
    // the depth at which it was computed, the type of bound (exact, lower,
    // or upper), and the best move found.  The key is the Zobrist hash
    // returned by Board::getHashKey().  Using a transposition table
    // greatly reduces redundant computation by caching the results of
    // previously evaluated positions.
    struct TTEntry {
        int depth;
        int score;
        int flag; // 0 = exact, 1 = lower bound, 2 = upper bound
        Move bestMove;
    };
    std::unordered_map<uint64_t, TTEntry> transTable;

    // --- Killer move heuristics ---
    // Killer moves are moves that caused a beta cutoff at a given search ply.
    // For each ply in the current search tree we store up to two killer moves
    // that will be tried early in move ordering.  We reset these on each new
    // search.
    static const int MAX_PLY = 64;
    Move killerMoves[MAX_PLY][2];

    // History heuristic table.  Records how often moves cause cutoffs to
    // further improve move ordering.  It is reset at the start of each
    // search.
    HistoryHeuristic history;
};

} // namespace gomoku

#endif // GOMOKU_SEARCH_H