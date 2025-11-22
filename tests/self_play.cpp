/**
 * Self‑play test harness for the Gomoku engine.
 *
 * This utility links directly against the existing board and search engine
 * implementation to simulate a full game where the engine plays both
 * sides.  After each move, the current board state is printed to the
 * terminal using simple ASCII graphics.  Black stones are shown as
 * ‘B’, white stones as ‘W’ and empty cells as ‘.’.  Coordinates are
 * labeled along the top and left edges to aid human readability.
 *
 * The starting position uses the standard fixed opening cross: white
 * stones at (6,6) and (5,5) and black stones at (6,5) and (5,6).  The
 * engine plays until one side achieves five in a row or the board
 * becomes full.  A per‑move time limit can be adjusted via the
 * constant TIME_LIMIT_MS below.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <locale>

#include "board.h"
#include "search.h"

using namespace gomoku;

namespace {

// Per‑move time limit in milliseconds.  Adjust this value to
// shorten or lengthen the thinking time allowed for each move.
static const int TIME_LIMIT_MS = 2000;

// Helper function to print the current board state.  It labels the
// columns with x‑coordinates along the top and rows with y‑coordinates
// along the left.  Empty squares are denoted by '.', black stones by
// 'B' and white stones by 'W'.
void printBoard(const Board &board) {
    // Print column headers
    std::cout << "   ";
    for (int x = 0; x < 12; ++x) {
        // Align single‑digit numbers for neatness
        if (x < 10) std::cout << ' ';
        std::cout << x << ' ';
    }
    std::cout << '\n';
    for (int y = 0; y < 12; ++y) {
        // Row header
        if (y < 10) std::cout << ' ';
        std::cout << y << ' ';
        for (int x = 0; x < 12; ++x) {
            int cell = board.getCellState(x, y);
            char c = '.';
            if (cell == 1)      c = 'B';
            else if (cell == 2) c = 'W';
            std::cout << ' ' << c;
        }
        std::cout << '\n';
    }
}

} // unnamed namespace

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::cout.imbue(std::locale());
    Board board;
    SearchEngine engine;
    int moveCount = 0;

    std::cout << "Starting self‑play game. Initial board:\n";
    printBoard(board);
    std::cout << std::flush;

    // Continue until one player wins or the board is full.
    while (true) {
        // Determine current player
        Player current = board.sideToMove();
        // Check if either player has already won (possible due to forced
        // win detection routines inside SearchEngine).  Also handle
        // completion when the board has no legal moves left.
        bool blackWin = board.checkWin(Player::Black);
        bool whiteWin = board.checkWin(Player::White);
        if (blackWin || whiteWin) {
            std::cout << (blackWin ? "Black" : "White") << " wins!\n";
            break;
        }
        auto legal = board.getLegalMoves();
        if (legal.empty()) {
            std::cout << "No more legal moves – draw or stalemate.\n";
            break;
        }

        // Find the best move for the current player.  We pass the
        // current player as myColor so the evaluation is from that
        // player's perspective.  The time limit is fixed per move.
        Move mv = engine.findBestMove(board, current, TIME_LIMIT_MS);
        // If the engine returns an invalid move or chooses an occupied
        // square, fall back to the first candidate move.  This guards
        // against unexpected failures in the search code.
        if (mv.x < 0 || mv.y < 0 || board.isOccupied(mv.x, mv.y)) {
            if (!legal.empty()) {
                mv = legal.front();
            } else {
                std::cout << "No candidate moves available.\n";
                break;
            }
        }
        // Apply the move to the board.
        board.makeMove(mv.x, mv.y);
        ++moveCount;
        std::cout << "Move " << moveCount << ": "
                  << (current == Player::Black ? "Black" : "White")
                  << " plays (" << mv.x << "," << mv.y << ")\n";
        printBoard(board);
        std::cout << std::flush;

        // Brief pause between moves for readability.  Comment out if
        // continuous output is preferred.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}