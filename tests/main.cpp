#include <iostream>
#include <string>
#include <sstream>

#include "board.h"
#include "search.h"

using namespace gomoku;

// Entry point for the Gomoku engine implementing the competition protocol.
// The engine reads commands from standard input and outputs responses to
// standard output.  Supported commands are:
//   START <field>   – Initialize the game; field = 1 means we are black,
//                     field = 2 means we are white.  Respond with "OK".
//   PLACE x y       – The opponent has played at position (x, y).
//   TURN            – It is our turn to move; output our move as "x y".
//   END <field>     – The game has ended; <field> indicates the winner.
//   DEBUG ...       – A debug command; ignored by this implementation.
int main() {
    Board board;
    SearchEngine engine;
    Player myColor = Player::Black;
    std::string token;
    while (std::cin >> token) {
        if (token == "START") {
            int field;
            if (!(std::cin >> field)) break;
            // field == 1 -> we are black; field == 2 -> we are white
            myColor = (field == 1 ? Player::Black : Player::White);
            // Reset the board to the initial predetermined state.
            board = Board();
            std::cout << "OK" << std::endl;
            std::cout.flush();
        } else if (token == "PLACE") {
            int x, y;
            if (!(std::cin >> x >> y)) break;
            board.makeMove(x, y);
        } else if (token == "TURN") {
            // Compute and output our move.
            // Use a time limit of 1800 milliseconds per move.
            Move myMove = engine.findBestMove(board, myColor, 1800);
            if (myMove.x < 0 || myMove.y < 0) {
                // Fallback: if no move found, pick the first candidate.
                auto candidates = board.getCandidateMoves();
                if (!candidates.empty()) {
                    myMove = candidates.front();
                } else {
                    // No legal moves
                    myMove = Move(0, 0);
                }
            }
            board.makeMove(myMove.x, myMove.y);
            std::cout << myMove.x << " " << myMove.y << std::endl;
            std::cout.flush();
        } else if (token == "END") {
            int field;
            if (std::cin >> field) {
                // Game ended; we could log results or reset.
                // For this implementation we simply break out.
                break;
            }
        } else if (token == "DEBUG") {
            // Consume the rest of the line for debug messages.
            std::string line;
            std::getline(std::cin, line);
            // Optionally print debug info to stderr.
            // std::cerr << "DEBUG:" << line << std::endl;
        } else {
            // Unknown command; consume the rest of the line to avoid
            // desynchronization.
            std::string line;
            std::getline(std::cin, line);
        }
    }
    return 0;
}