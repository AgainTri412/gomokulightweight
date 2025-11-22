#include "board.h"
#include <cstring>
#include <random>

namespace gomoku {

// The Board implementation uses two bitboards (one for each player) split into three
// 64‑bit chunks to cover all 144 squares of a 12×12 board.  A cell at
// coordinate (x,y) has linear index idx = y*12 + x.  The high bits of idx
// (idx >> 6) select which 64‑bit chunk stores that cell; the low bits
// (idx & 63) select the bit within the chunk.  Each bitboard tracks only
// that player’s stones; a move simply sets or clears the bit for the
// appropriate player and toggles side_to_move.  The Board also maintains a
// Zobrist hash key that encodes the entire position (including side to move).

// Static member definitions for Zobrist hashing.  These will be
// initialized when the first Board is constructed.
bool Board::zobristInitialized = false;
uint64_t Board::zobristTable[12][12][2];
uint64_t Board::zobristSide = 0ULL;

Board::Board() : side_to_move(Player::Black) {
    // Ensure Zobrist tables are initialized before using them.
    initZobrist();
    // Initialize bitboards to zero and hashKey to zero.
    std::memset(bb, 0, sizeof(bb));
    hashKey = 0ULL;

    // Starting position: white at (6,6) and (5,5),
    // black at (6,5) and (5,6).
    // Place white stones and update hash.
    {
        int positions[][2] = {{6,6}, {5,5}};
        for (auto &p : positions) {
            int idx = index(p[0], p[1]);
            int c = chunkOf(idx);
            int off = offsetOf(idx);
            bb[static_cast<int>(Player::White)][c] |= (1ULL << off);
            // Update hash for white stone at (x,y).
            hashKey ^= zobristTable[p[0]][p[1]][static_cast<int>(Player::White)];
        }
    }
    // Place black stones and update hash.
    {
        int positions[][2] = {{6,5}, {5,6}};
        for (auto &p : positions) {
            int idx = index(p[0], p[1]);
            int c = chunkOf(idx);
            int off = offsetOf(idx);
            bb[static_cast<int>(Player::Black)][c] |= (1ULL << off);
            // Update hash for black stone at (x,y).
            hashKey ^= zobristTable[p[0]][p[1]][static_cast<int>(Player::Black)];
        }
    }
    // It is black's turn to move by convention; no need to toggle side marker.
    side_to_move = Player::Black;
}

bool Board::isOccupied(int x, int y) const {
    if (x < 0 || x >= 12 || y < 0 || y >= 12) return true;
    int idx = index(x, y);
    int c = chunkOf(idx);
    int off = offsetOf(idx);
    uint64_t mask = 1ULL << off;
    return ((bb[0][c] | bb[1][c]) & mask) != 0ULL;
}

// Initialize the Zobrist tables with random 64‑bit numbers.  This function
// is called lazily by the Board constructor.  A fixed seed is used to
// ensure reproducibility across runs.  Each cell and player combination
// receives a unique random number, and another random number is used for
// the side‑to‑move marker.
void Board::initZobrist() {
    if (zobristInitialized) return;
    // Use a pseudo‑random number generator with a fixed seed for
    // deterministic behavior.  This avoids differences in play between
    // processes due to different random seeds.
    std::mt19937_64 rng(0x12345678ABCDEFULL);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            for (int p = 0; p < 2; ++p) {
                zobristTable[x][y][p] = rng();
            }
        }
    }
    zobristSide = rng();
    zobristInitialized = true;
}

bool Board::makeMove(int x, int y) {
    // Reject moves off the board.
    if (x < 0 || x >= 12 || y < 0 || y >= 12) return false;
    // Reject moves on occupied squares.
    if (isOccupied(x, y)) {
        return false;
    }
    // Compute the linear index and locate the 64‑bit chunk and bit offset.
    int idx = index(x, y);
    int c   = chunkOf(idx);
    int off = offsetOf(idx);
    // Convert the enum Player to an integer 0 (Black) or 1 (White).
    int playerIndex = static_cast<int>(side_to_move);
    // Set the bit in the current player's bitboard.
    bb[playerIndex][c] |= (1ULL << off);
    // Update the Zobrist hash: XOR the random value associated with this
    // cell and player.  Toggling a bit on or off is achieved by XORing
    // the same value in makeMove and unmakeMove.
    hashKey ^= zobristTable[x][y][playerIndex];
    // Toggle side_to_move and update the side marker in the hash.  The side
    // marker ensures that the same board position with different players to
    // move yields a different hash key.
    side_to_move = (side_to_move == Player::Black) ? Player::White : Player::Black;
    hashKey ^= zobristSide;
    return true;
}

bool Board::unmakeMove(int x, int y) {
    // Restore side_to_move by toggling it back and updating the side
    // marker in the hash.  This is the reverse operation of makeMove().
    side_to_move = (side_to_move == Player::Black) ? Player::White : Player::Black;
    hashKey ^= zobristSide;
    // Locate the stone's bit in the bitboard.
    int idx  = index(x, y);
    int c    = chunkOf(idx);
    int off  = offsetOf(idx);
    uint64_t mask = 1ULL << off;
    int p    = static_cast<int>(side_to_move);
    // Clear the bit from the appropriate player's bitboard.
    bb[p][c] &= ~mask;
    // XOR the corresponding random number to remove the stone from the hash.
    hashKey ^= zobristTable[x][y][p];
    return true;
}

bool Board::checkWin(Player player) const {
    int p = static_cast<int>(player);
    // Iterate over every occupied cell of player p.  For each stone we
    // attempt to count contiguous stones in four directions.  As soon as
    // we find a line of length ≥ 5 we can declare a win.
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            int idx = index(x, y);
            int c   = chunkOf(idx);
            int off = offsetOf(idx);
            // Skip if this cell does not contain a stone of player p.
            if ((bb[p][c] & (1ULL << off)) == 0ULL) {
                continue;
            }
            // For each stone, check four directions: horizontal (1,0),
            // vertical (0,1), diagonal down‑right (1,1) and diagonal
            // up‑right (1,−1).  We count contiguous stones in both
            // directions and sum the lengths.
            const int dirs[4][2] = {
                {1, 0},  // horizontal
                {0, 1},  // vertical
                {1, 1},  // diagonal down‑right
                {1, -1}  // diagonal up‑right
            };
            for (int d = 0; d < 4; ++d) {
                int dx = dirs[d][0];
                int dy = dirs[d][1];
                int count = 1;
                // Extend forward in direction (dx,dy).
                int nx = x + dx;
                int ny = y + dy;
                while (nx >= 0 && nx < 12 && ny >= 0 && ny < 12) {
                    int nidx = index(nx, ny);
                    int nc   = chunkOf(nidx);
                    int no   = offsetOf(nidx);
                    if ((bb[p][nc] & (1ULL << no)) == 0ULL) break;
                    ++count;
                    nx += dx;
                    ny += dy;
                }
                // Extend backward in direction (−dx,−dy).
                nx = x - dx;
                ny = y - dy;
                while (nx >= 0 && nx < 12 && ny >= 0 && ny < 12) {
                    int nidx = index(nx, ny);
                    int nc   = chunkOf(nidx);
                    int no   = offsetOf(nidx);
                    if ((bb[p][nc] & (1ULL << no)) == 0ULL) break;
                    ++count;
                    nx -= dx;
                    ny -= dy;
                }
                // If the total count reaches 5 or more, this player wins.
                if (count >= 5) return true;
            }
        }
    }
    return false;
}

std::vector<Move> Board::getLegalMoves() const {
    std::vector<Move> moves;
    moves.reserve(144);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            if (!isOccupied(x, y)) {
                moves.emplace_back(x, y);
            }
        }
    }
    return moves;
}

std::vector<Move> Board::getCandidateMoves() const {
    std::vector<Move> moves;
    // Compute a minimal axis‑aligned bounding box around all existing stones.
    int min_x = 12, max_x = -1, min_y = 12, max_y = -1;
    bool found = false;
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            if (isOccupied(x, y)) {
                found = true;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    if (!found) {
        // No stones on the board: play in the centre.  On a 12×12 board the
        // true centre is (5,5) (zero‑indexed).  We return only this move
        // since all other moves are equivalent by symmetry.
        moves.emplace_back(5, 5);
        return moves;
    }
    // Expand the bounding box by a margin of two cells in all directions.
    // This focuses search near the existing stones but allows for tactical
    // moves slightly away from the cluster.  Clamp the box to the board.
    min_x = std::max(0, min_x - 2);
    min_y = std::max(0, min_y - 2);
    max_x = std::min(11, max_x + 2);
    max_y = std::min(11, max_y + 2);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            if (!isOccupied(x, y)) {
                bool neighbor = false;
                // Only keep cells that have at least one adjacent stone.
                // Moves far from existing stones are unlikely to matter in
                // practice and can be ignored to reduce the branching factor.
                for (int dy = -1; dy <= 1 && !neighbor; ++dy) {
                    for (int dx = -1; dx <= 1 && !neighbor; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < 12 && ny >= 0 && ny < 12) {
                            if (isOccupied(nx, ny)) {
                                neighbor = true;
                            }
                        }
                    }
                }
                if (neighbor) {
                    moves.emplace_back(x, y);
                }
            }
        }
    }
    // Rare case: if the bounding box produced no moves (e.g. stones are
    // isolated with margin=2), fall back to all legal moves.  This ensures
    // that the engine always has moves to consider.
    if (moves.empty()) {
        return getLegalMoves();
    }
    return moves;
}

int Board::getCellState(int x, int y) const {
    if (x < 0 || x >= 12 || y < 0 || y >= 12) return -1;
    int idx = index(x, y);
    int c = chunkOf(idx);
    int off = offsetOf(idx);
    uint64_t mask = 1ULL << off;
    if ((bb[static_cast<int>(Player::Black)][c] & mask) != 0ULL) {
        return 1;
    }
    if ((bb[static_cast<int>(Player::White)][c] & mask) != 0ULL) {
        return 2;
    }
    return 0;
}

int Board::countStones(Player player) const {
    int p = static_cast<int>(player);
    int total = 0;
    // Use a portable popcount implementation to avoid relying on compiler
    // built‑ins that might not be available on all compilers.  Brian
    // Kernighan’s algorithm counts the number of set bits in a 64‑bit
    // integer by repeatedly clearing the least significant set bit.
    auto popcount64 = [](uint64_t x) {
        int cnt = 0;
        while (x) {
            x &= (x - 1);
            ++cnt;
        }
        return cnt;
    };
    for (int c = 0; c < 3; ++c) {
        total += popcount64(bb[p][c]);
    }
    return total;
}

} // namespace gomoku