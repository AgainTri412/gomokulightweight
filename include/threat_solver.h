// threat_solver.h
// Pattern-based threat detection for Gomoku.
//
// The threat solver focuses on classic Gomoku tactics: keeping the board open
// for five-in-a-row while denying the opponent open fours, open threes, and
// broken threes. An "open" pattern keeps empty cells at both ends; a "broken"
// three contains a single gap but can still produce an open four in one move.
// By scanning every line (rows, columns, and both diagonals) and scoring
// dangerous layouts, the solver proposes blocking moves that neutralize the
// opponent's most urgent threats. This mirrors common defensive strategy in
// Gomoku: stop imminent wins first, then close off flexible threes before they
// blossom into lethal fours.

#ifndef GOMOKU_THREAT_SOLVER_H
#define GOMOKU_THREAT_SOLVER_H

#include <vector>
#include <unordered_map>

#include "board.h"

namespace gomoku {

// Reports blocking moves against tactical threats created by the opponent of
// the specified defender. Each suggestion includes a severity score so callers
// can prioritize urgent defenses (e.g., open fours) over lower-risk patterns
// like closed threes.
class ThreatSolver {
public:
    struct ThreatMove {
        Move move;
        int severity;
    };

    // Identify blocking moves that defend against the opponent's tactical
    // threats. The defender argument indicates whose interests are protected;
    // the solver analyzes patterns belonging to the opponent of defender.
    std::vector<ThreatMove> findBlockingMoves(const Board &board, Player defender) const;

private:
    struct LineContext {
        int xStart;
        int yStart;
        int dx;
        int dy;
        int length;
    };

    // Convert the board to a 2D grid of ints: 1 for the attacking side, -1 for
    // the defender, 0 for empty.
    void buildGrid(const Board &board, Player defender, int grid[12][12]) const;
    void processLine(const int grid[12][12], const LineContext &ctx,
                     std::unordered_map<int, int> &bestSeverity) const;
    void addMoveIfStronger(int x, int y, int severity,
                           std::unordered_map<int, int> &bestSeverity) const;
};

} // namespace gomoku

#endif // GOMOKU_THREAT_SOLVER_H
