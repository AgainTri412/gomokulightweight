#include "threat_solver.h"

#include <algorithm>
#include <unordered_map>

namespace gomoku {

namespace {
// Helper to convert board coordinates to a single integer key for maps.
inline int coordKey(int x, int y) {
    return y * 12 + x;
}
}

void ThreatSolver::buildGrid(const Board &board, Player defender, int grid[12][12]) const {
    Player attacker = (defender == Player::Black ? Player::White : Player::Black);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            int state = board.getCellState(x, y);
            if ((state == 1 && attacker == Player::Black) || (state == 2 && attacker == Player::White)) {
                grid[y][x] = 1;      // attacker stone
            } else if ((state == 1 && defender == Player::Black) || (state == 2 && defender == Player::White)) {
                grid[y][x] = -1;     // defender stone
            } else {
                grid[y][x] = 0;      // empty
            }
        }
    }
}

void ThreatSolver::addMoveIfStronger(int x, int y, int severity,
                                     std::unordered_map<int, int> &bestSeverity) const {
    if (x < 0 || x >= 12 || y < 0 || y >= 12) return;
    int key = coordKey(x, y);
    auto it = bestSeverity.find(key);
    if (it == bestSeverity.end() || severity > it->second) {
        bestSeverity[key] = severity;
    }
}

void ThreatSolver::processLine(const int grid[12][12], const LineContext &ctx,
                               std::unordered_map<int, int> &bestSeverity) const {
    const int OPEN_FOUR = 1000000;      // Immediate loss if not blocked.
    const int SIMPLE_FOUR = 500000;     // One-sided four still urgent.
    const int OPEN_THREE = 120000;      // Two-sided three.
    const int BROKEN_THREE = 60000;     // Gap three with at least one open end.

    auto cellAt = [&](int offset) {
        int x = ctx.xStart + offset * ctx.dx;
        int y = ctx.yStart + offset * ctx.dy;
        return grid[y][x];
    };

    for (int i = 0; i + 5 <= ctx.length; ++i) {
        int defenderStones = 0;
        int attackerStones = 0;
        int empties = 0;
        int emptyOffsets[5];
        int emptyCount = 0;
        for (int j = 0; j < 5; ++j) {
            int val = cellAt(i + j);
            if (val == -1) {
                defenderStones++;
            } else if (val == 1) {
                attackerStones++;
            } else {
                empties++;
                emptyOffsets[emptyCount++] = j;
            }
        }
        if (defenderStones > 0) continue; // Blocked window.

        bool leftOpen = (i - 1 >= 0 && cellAt(i - 1) == 0);
        bool rightOpen = (i + 5 < ctx.length && cellAt(i + 5) == 0);

        if (attackerStones == 4 && empties == 1) {
            // Four-in-a-row with no defender interference. Block the lone empty.
            int offset = emptyOffsets[0];
            int x = ctx.xStart + (i + offset) * ctx.dx;
            int y = ctx.yStart + (i + offset) * ctx.dy;
            int severity = (leftOpen && rightOpen) ? OPEN_FOUR : SIMPLE_FOUR;
            addMoveIfStronger(x, y, severity, bestSeverity);
        } else if (attackerStones == 3 && empties == 2) {
            // Potential open or broken three.
            int severity;
            if (leftOpen && rightOpen) {
                severity = OPEN_THREE;
            } else if (leftOpen || rightOpen) {
                severity = BROKEN_THREE;
            } else {
                continue;
            }
            for (int k = 0; k < emptyCount; ++k) {
                int offset = emptyOffsets[k];
                int x = ctx.xStart + (i + offset) * ctx.dx;
                int y = ctx.yStart + (i + offset) * ctx.dy;
                addMoveIfStronger(x, y, severity, bestSeverity);
            }
        }
    }
}

std::vector<ThreatSolver::ThreatMove> ThreatSolver::findBlockingMoves(const Board &board, Player defender) const {
    int grid[12][12];
    buildGrid(board, defender, grid);
    std::unordered_map<int, int> bestSeverity;

    // Horizontal lines.
    for (int y = 0; y < 12; ++y) {
        LineContext ctx{0, y, 1, 0, 12};
        processLine(grid, ctx, bestSeverity);
    }
    // Vertical lines.
    for (int x = 0; x < 12; ++x) {
        LineContext ctx{x, 0, 0, 1, 12};
        processLine(grid, ctx, bestSeverity);
    }
    // Diagonals (top-left to bottom-right).
    for (int k = -11; k <= 11; ++k) {
        int xStart = std::max(0, k);
        int yStart = std::max(0, -k);
        int len = std::min(12 - xStart, 12 - yStart);
        LineContext ctx{xStart, yStart, 1, 1, len};
        processLine(grid, ctx, bestSeverity);
    }
    // Anti-diagonals (top-right to bottom-left).
    for (int s = 0; s <= 22; ++s) {
        int xStart = std::max(0, s - 11);
        int yStart = std::min(11, s);
        int len = std::min(yStart + 1, 12 - xStart);
        LineContext ctx{xStart, yStart, 1, -1, len};
        processLine(grid, ctx, bestSeverity);
    }

    std::vector<ThreatMove> result;
    result.reserve(bestSeverity.size());
    for (const auto &entry : bestSeverity) {
        int key = entry.first;
        int y = key / 12;
        int x = key % 12;
        result.push_back({Move(x, y), entry.second});
    }
    std::sort(result.begin(), result.end(), [](const ThreatMove &a, const ThreatMove &b) {
        return a.severity > b.severity;
    });
    return result;
}

} // namespace gomoku
