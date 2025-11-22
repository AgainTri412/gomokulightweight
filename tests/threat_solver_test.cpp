#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "board.h"
#include "threat_solver.h"

using namespace gomoku;

namespace {

// Pre-chosen filler moves far from the tested patterns so we can alternate turns
// without disturbing the threat windows.
const std::vector<Move> kFillerMoves = {
    {11, 0}, {11, 1}, {11, 2}, {11, 3}, {11, 4}, {11, 5}, {11, 6}, {11, 7},
    {11, 8}, {11, 9}, {10, 0}, {10, 1}, {10, 2}, {10, 3}, {10, 4}, {10, 5},
    {10, 6}, {10, 7}, {10, 8}, {10, 9}
};

// Helper to place a stone for the desired player. If it is not the desired
// player's turn, we consume filler moves to toggle the turn without affecting
// the tested area.
void placeStoneFor(Board &board, Player player, int x, int y, size_t &fillerIndex) {
    while (board.sideToMove() != player) {
        assert(fillerIndex < kFillerMoves.size());
        const Move filler = kFillerMoves[fillerIndex++];
        bool fillerPlaced = board.makeMove(filler.x, filler.y);
        assert(fillerPlaced);
    }
    bool placed = board.makeMove(x, y);
    assert(placed);
}

int findSeverity(const std::vector<ThreatSolver::ThreatMove> &moves, int x, int y) {
    for (const auto &m : moves) {
        if (m.move.x == x && m.move.y == y) {
            return m.severity;
        }
    }
    return 0;
}

struct TestRunner {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << "Test failed: " << message << "\n";
        }
    }
};

} // namespace

void testNoThreats(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    auto result = solver.findBlockingMoves(board, Player::Black);
    tr.check(result.empty(), "Initial board should have no urgent threats for black to block.");
}

void testOpenFourBlocksBothEnds(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    size_t fillerIndex = 0;
    // Attacker is White (defender is Black). Create an open four: . W W W W .
    placeStoneFor(board, Player::White, 3, 0, fillerIndex);
    placeStoneFor(board, Player::White, 4, 0, fillerIndex);
    placeStoneFor(board, Player::White, 5, 0, fillerIndex);
    placeStoneFor(board, Player::White, 6, 0, fillerIndex);

    auto moves = solver.findBlockingMoves(board, Player::Black);
    tr.check(moves.size() >= 2, "Open four should surface both end blocks.");
    tr.check(findSeverity(moves, 2, 0) == 1000000, "Left end of open four should be severity OPEN_FOUR.");
    tr.check(findSeverity(moves, 7, 0) == 1000000, "Right end of open four should be severity OPEN_FOUR.");
    if (!moves.empty()) {
        tr.check(moves.front().severity >= moves.back().severity, "Threat moves should be sorted descending by severity.");
    }
}

void testSimpleFourHasSingleBlock(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    size_t fillerIndex = 0;
    // Attacker is White; pattern at top edge yields a one-sided four: X W W W W .
    placeStoneFor(board, Player::White, 0, 1, fillerIndex);
    placeStoneFor(board, Player::White, 1, 1, fillerIndex);
    placeStoneFor(board, Player::White, 2, 1, fillerIndex);
    placeStoneFor(board, Player::White, 3, 1, fillerIndex);

    auto moves = solver.findBlockingMoves(board, Player::Black);
    tr.check(findSeverity(moves, 4, 1) == 500000, "Closed-side four should have SIMPLE_FOUR severity at the lone empty.");
}

void testOpenThreeFindsBothGaps(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    size_t fillerIndex = 0;
    // Attacker is Black; create 0 1 2 3 4 5 6 7 8 positions:
    // ... B B B ... so a window with empty neighbors on both ends.
    placeStoneFor(board, Player::Black, 4, 2, fillerIndex);
    placeStoneFor(board, Player::Black, 5, 2, fillerIndex);
    placeStoneFor(board, Player::Black, 6, 2, fillerIndex);

    auto moves = solver.findBlockingMoves(board, Player::White);
    tr.check(findSeverity(moves, 3, 2) == 120000, "Open three should flag the left empty with OPEN_THREE severity.");
    tr.check(findSeverity(moves, 7, 2) == 120000, "Open three should flag the right empty with OPEN_THREE severity.");
}

void testBrokenThreeOneOpenEnd(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    size_t fillerIndex = 0;
    // Attacker is White; pattern with only one open end (right side is open).
    // A black stone at (2,8) closes the left side outside the scanned window.
    placeStoneFor(board, Player::Black, 2, 8, fillerIndex); // defender stone outside the window.
    placeStoneFor(board, Player::White, 3, 8, fillerIndex);
    placeStoneFor(board, Player::White, 4, 8, fillerIndex);
    placeStoneFor(board, Player::White, 6, 8, fillerIndex);

    auto moves = solver.findBlockingMoves(board, Player::Black);
    tr.check(findSeverity(moves, 5, 8) == 60000, "Broken three should include the internal gap with BROKEN_THREE severity when only one end is open.");
    tr.check(findSeverity(moves, 7, 8) == 60000, "Broken three should include the trailing empty at the single open end.");
}

void testSeverityKeepsBestValuePerCell(TestRunner &tr) {
    Board board;
    ThreatSolver solver;
    size_t fillerIndex = 0;
    // Attacker is White. Vertical one-sided four ending at (4,11).
    placeStoneFor(board, Player::White, 4, 7, fillerIndex);
    placeStoneFor(board, Player::White, 4, 8, fillerIndex);
    placeStoneFor(board, Player::White, 4, 9, fillerIndex);
    placeStoneFor(board, Player::White, 4, 10, fillerIndex);
    // Horizontal open three along row 11 that also includes (4,11).
    placeStoneFor(board, Player::White, 5, 11, fillerIndex);
    placeStoneFor(board, Player::White, 6, 11, fillerIndex);
    placeStoneFor(board, Player::White, 7, 11, fillerIndex);

    auto moves = solver.findBlockingMoves(board, Player::Black);
    int severityAtShared = findSeverity(moves, 4, 11);
    tr.check(severityAtShared == 500000, "Cell shared by multiple threats should keep the higher SIMPLE_FOUR severity over OPEN_THREE.");
    tr.check(findSeverity(moves, 8, 11) == 120000, "Other open-three endpoint should retain OPEN_THREE severity.");
}

int main() {
    TestRunner tr;
    testNoThreats(tr);
    testOpenFourBlocksBothEnds(tr);
    testSimpleFourHasSingleBlock(tr);
    testOpenThreeFindsBothGaps(tr);
    testBrokenThreeOneOpenEnd(tr);
    testSeverityKeepsBestValuePerCell(tr);

    if (tr.failures == 0) {
        std::cout << "All ThreatSolver tests passed." << std::endl;
        return 0;
    }
    std::cerr << tr.failures << " test(s) failed." << std::endl;
    return 1;
}

