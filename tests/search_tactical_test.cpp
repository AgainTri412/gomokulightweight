#include <cassert>
#include <iostream>
#include <vector>

#include "board.h"
#include "search.h"

using namespace gomoku;

namespace {

// Filler moves to toggle turns without disturbing central patterns.
const std::vector<Move> kFillerMoves = {
    {11, 0}, {11, 1}, {11, 2}, {11, 3}, {11, 4}, {11, 5}, {11, 6}, {11, 7},
    {11, 8}, {11, 9}, {10, 0}, {10, 1}, {10, 2}, {10, 3}, {10, 4}, {10, 5},
    {10, 6}, {10, 7}, {10, 8}, {10, 9}
};

struct TestRunner {
    int failures = 0;
    void check(bool cond, const std::string &msg) {
        if (!cond) {
            ++failures;
            std::cerr << "Test failed: " << msg << "\n";
        }
    }
};

// Place a stone for the given player, consuming filler moves to maintain turn order.
void placeStoneFor(Board &board, Player player, int x, int y, size_t &fillerIndex) {
    while (board.sideToMove() != player) {
        assert(fillerIndex < kFillerMoves.size());
        bool placed = board.makeMove(kFillerMoves[fillerIndex].x, kFillerMoves[fillerIndex].y);
        assert(placed);
        ++fillerIndex;
    }
    bool ok = board.makeMove(x, y);
    assert(ok);
}

// Force the side to move to the requested player using filler moves.
void syncTurn(Board &board, Player desired, size_t &fillerIndex) {
    while (board.sideToMove() != desired) {
        assert(fillerIndex < kFillerMoves.size());
        bool placed = board.makeMove(kFillerMoves[fillerIndex].x, kFillerMoves[fillerIndex].y);
        assert(placed);
        ++fillerIndex;
    }
}

} // namespace

void testImmediateWinChosen(TestRunner &tr) {
    Board board;
    SearchEngine engine;
    size_t fillerIndex = 0;

    // White has a four-in-a-row on row 4: W at (5,4)(6,4)(7,4)(8,4) and Black blocks the right end at (9,4).
    placeStoneFor(board, Player::White, 5, 4, fillerIndex);
    placeStoneFor(board, Player::White, 6, 4, fillerIndex);
    placeStoneFor(board, Player::White, 7, 4, fillerIndex);
    placeStoneFor(board, Player::White, 8, 4, fillerIndex);
    placeStoneFor(board, Player::Black, 9, 4, fillerIndex);
    syncTurn(board, Player::White, fillerIndex);

    Move best = engine.findBestMove(board, Player::White, 500);
    tr.check(best.x == 4 && best.y == 4, "Engine should play the unique winning move to complete five in a row.");
}

void testBlocksOpponentImmediateWin(TestRunner &tr) {
    Board board;
    SearchEngine engine;
    size_t fillerIndex = 0;

    // Black forms a diagonal four with one open end at (5,8); the far end (10,3) is blocked by White.
    placeStoneFor(board, Player::Black, 6, 7, fillerIndex);
    placeStoneFor(board, Player::Black, 7, 6, fillerIndex);
    placeStoneFor(board, Player::Black, 8, 5, fillerIndex);
    placeStoneFor(board, Player::Black, 9, 4, fillerIndex);
    placeStoneFor(board, Player::White, 10, 3, fillerIndex); // close the upper end
    syncTurn(board, Player::White, fillerIndex);

    Move best = engine.findBestMove(board, Player::White, 500);
    tr.check(best.x == 5 && best.y == 8, "Engine should block opponent's immediate win at the only open end.");
}

int main() {
    TestRunner tr;
    testImmediateWinChosen(tr);
    testBlocksOpponentImmediateWin(tr);

    if (tr.failures == 0) {
        std::cout << "All tactical search tests passed." << std::endl;
        return 0;
    }
    std::cerr << tr.failures << " test(s) failed." << std::endl;
    return 1;
}

