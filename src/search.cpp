#include "search.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <utility>

// Include history heuristic for move ordering.
#include "history_heuristic.h"

namespace gomoku {

SearchEngine::SearchEngine() : maxDepthReached(0) {
}

// Determine whether an opening move should be played for the current position.
// This simple opening book looks for the very first move (after the
// predetermined starting stones) and chooses a central point that expands
// the initial cross.  For Black, it recommends playing on the diagonal at
// (7,7).  For White or later positions, no opening move is returned.
bool SearchEngine::getOpeningMove(const Board &board, Player myColor, Move &outMove) const {
    // Count total stones on the board.
    int total = board.countStones(Player::Black) + board.countStones(Player::White);
    // The predetermined position has 4 stones.  We provide a book move only
    // immediately after this position if it is our turn.
    if (total == 4 && board.sideToMove() == myColor) {
        // Only implement an opening move for the player with the first move
        // advantage (black).  White will rely on search.
        if (myColor == Player::Black) {
            // Choose a point diagonally away from the central cross.
            // (7,7) is one such point.  If occupied (rare), try (4,7), (7,4), (4,4).
            const Move preferred[] = { Move(7,7), Move(7,4), Move(4,7), Move(4,4) };
            for (const auto &m : preferred) {
                if (!board.isOccupied(m.x, m.y)) {
                    outMove = m;
                    return true;
                }
            }
        }
    }
    return false;
}

void SearchEngine::startTimer(int timeLimitMs) {
    timeEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeLimitMs);
    maxDepthReached = 0;
}

bool SearchEngine::timeUp() const {
    return std::chrono::steady_clock::now() >= timeEnd;
}

int SearchEngine::patternScore(int count, bool leftOpen, bool rightOpen) const {
    // Assign scores for different pattern types.  Higher numbers reflect
    // stronger threats.  These values can be tuned for better play.
    // Threat weights.  Larger values correspond to stronger threats.
    const int SCORE_FIVE           = 100000000;
    const int SCORE_OPEN_FOUR      = 10000000;
    const int SCORE_SIMPLE_FOUR    = 1000000;
    const int SCORE_OPEN_THREE     = 100000;
    const int SCORE_BROKEN_THREE   = 10000;
    const int SCORE_OPEN_TWO       = 1000;
    const int SCORE_CLOSED_TWO     = 100;
    if (count >= 5) {
        return SCORE_FIVE;
    }
    if (count == 4) {
        if (leftOpen && rightOpen) return SCORE_OPEN_FOUR;
        if (leftOpen || rightOpen) return SCORE_SIMPLE_FOUR;
    }
    if (count == 3) {
        if (leftOpen && rightOpen) return SCORE_OPEN_THREE;
        // A three with only one open end is treated as a broken three.
        if (leftOpen || rightOpen) return SCORE_BROKEN_THREE;
    }
    if (count == 2) {
        if (leftOpen && rightOpen) return SCORE_OPEN_TWO;
        if (leftOpen || rightOpen) return SCORE_CLOSED_TWO;
    }
    return 0;
}

// Evaluate the board from a player's perspective by scanning rows,
// columns and both diagonals for contiguous runs of stones.  Only
// straight runs are considered; broken patterns (e.g., "xx.x") are
// not recognized explicitly but may still be partially credited.
SearchEngine::EvalResult SearchEngine::evaluatePlayer(const Board &board, Player player) const {
    // Convert the board bitboards into a 2D integer array for easier
    // scanning.  We use the values 1 for the target player's stones,
    // -1 for the opponent's stones and 0 for empty.  This representation
    // allows us to recognise contiguous runs by simple loops.
    int grid[12][12];
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            int state = board.getCellState(x, y);
            if (state == 0) {
                grid[y][x] = 0;
            } else if ((state == 1 && player == Player::Black) || (state == 2 && player == Player::White)) {
                // Current player's stone is represented by +1
                grid[y][x] = 1;
            } else {
                // Opponent's stone is represented by -1
                grid[y][x] = -1;
            }
        }
    }
    int score = 0;
    int longestRun = 0;
    int longestOpen = 0;
    bool hasOpenFourDouble = false;
    // --- Rows ---
    // Scan each row.  When we encounter a run of 1s, we measure its length
    // and check whether the ends are open (i.e. adjacent cells are empty).
    // The patternScore() function converts (count, leftOpen, rightOpen)
    // into a numerical threat value.
    for (int y = 0; y < 12; ++y) {
        int x = 0;
        while (x < 12) {
            if (grid[y][x] == 1) {
                int x1 = x;
                while (x < 12 && grid[y][x] == 1) ++x;
                int count = x - x1;
                bool leftOpen = (x1 - 1 >= 0 && grid[y][x1 - 1] == 0);
                bool rightOpen = (x < 12 && grid[y][x] == 0);
                int pscore = patternScore(count, leftOpen, rightOpen);
                score += pscore;
                int openEnds = (leftOpen ? 1 : 0) + (rightOpen ? 1 : 0);
                if (count > longestRun || (count == longestRun && openEnds > longestOpen)) {
                    longestRun = count;
                    longestOpen = openEnds;
                }
                if (count == 4 && leftOpen && rightOpen) {
                    hasOpenFourDouble = true;
                }
            } else {
                ++x;
            }
        }
    }
    // --- Columns ---
    for (int x = 0; x < 12; ++x) {
        int y = 0;
        while (y < 12) {
            if (grid[y][x] == 1) {
                int y1 = y;
                while (y < 12 && grid[y][x] == 1) ++y;
                int count = y - y1;
                bool leftOpen = (y1 - 1 >= 0 && grid[y1 - 1][x] == 0);
                bool rightOpen = (y < 12 && grid[y][x] == 0);
                int pscore = patternScore(count, leftOpen, rightOpen);
                score += pscore;
                int openEnds = (leftOpen ? 1 : 0) + (rightOpen ? 1 : 0);
                if (count > longestRun || (count == longestRun && openEnds > longestOpen)) {
                    longestRun = count;
                    longestOpen = openEnds;
                }
                if (count == 4 && leftOpen && rightOpen) {
                    hasOpenFourDouble = true;
                }
            } else {
                ++y;
            }
        }
    }
    // --- Diagonals (top‑left to bottom‑right) ---
    // For each diagonal index from -(rows-1) to (cols-1)
    for (int k = -11; k <= 11; ++k) {
        int xStart = std::max(0, k);
        int yStart = std::max(0, -k);
        int len = std::min(12 - xStart, 12 - yStart);
        int i = 0;
        while (i < len) {
            int xcur = xStart + i;
            int ycur = yStart + i;
            if (grid[ycur][xcur] == 1) {
                int j = i;
                while (j < len) {
                    int xj = xStart + j;
                    int yj = yStart + j;
                    if (grid[yj][xj] != 1) break;
                    ++j;
                }
                int count = j - i;
                bool leftOpen = false;
                if (i - 1 >= 0) {
                    int xl = xStart + (i - 1);
                    int yl = yStart + (i - 1);
                    if (grid[yl][xl] == 0) leftOpen = true;
                }
                bool rightOpen = false;
                if (j < len) {
                    int xr = xStart + j;
                    int yr = yStart + j;
                    if (grid[yr][xr] == 0) rightOpen = true;
                }
                int pscore = patternScore(count, leftOpen, rightOpen);
                score += pscore;
                int openEnds = (leftOpen ? 1 : 0) + (rightOpen ? 1 : 0);
                if (count > longestRun || (count == longestRun && openEnds > longestOpen)) {
                    longestRun = count;
                    longestOpen = openEnds;
                }
                if (count == 4 && leftOpen && rightOpen) {
                    hasOpenFourDouble = true;
                }
                i = j;
            } else {
                ++i;
            }
        }
    }
    // --- Anti‑diagonals (top‑right to bottom‑left) ---
    // For each anti-diagonal index (x+y) from 0 to 22
    for (int s = 0; s <= 22; ++s) {
        int xStart = std::max(0, s - 11);
        int yStart = std::min(11, s);
        int len = std::min(yStart + 1, 12 - xStart);
        int i = 0;
        while (i < len) {
            int xcur = xStart + i;
            int ycur = yStart - i;
            if (grid[ycur][xcur] == 1) {
                int j = i;
                while (j < len) {
                    int xj = xStart + j;
                    int yj = yStart - j;
                    if (grid[yj][xj] != 1) break;
                    ++j;
                }
                int count = j - i;
                bool leftOpen = false;
                if (i - 1 >= 0) {
                    int xl = xStart + (i - 1);
                    int yl = yStart - (i - 1);
                    if (grid[yl][xl] == 0) leftOpen = true;
                }
                bool rightOpen = false;
                if (j < len) {
                    int xr = xStart + j;
                    int yr = yStart - j;
                    if (grid[yr][xr] == 0) rightOpen = true;
                }
                int pscore = patternScore(count, leftOpen, rightOpen);
                score += pscore;
                int openEnds = (leftOpen ? 1 : 0) + (rightOpen ? 1 : 0);
                if (count > longestRun || (count == longestRun && openEnds > longestOpen)) {
                    longestRun = count;
                    longestOpen = openEnds;
                }
                if (count == 4 && leftOpen && rightOpen) {
                    hasOpenFourDouble = true;
                }
                i = j;
            } else {
                ++i;
            }
        }
    }
    EvalResult result;
    result.patternScore = score;
    result.longestRun = longestRun;
    result.longestOpenEnds = longestOpen;
    result.hasOpenFourDouble = hasOpenFourDouble;
    return result;
}

int SearchEngine::evaluate(const Board &board, Player myColor) const {
    // Evaluate the board as the difference between the current player's
    // pattern score and the opponent's pattern score.  A positive value
    // indicates that myColor has more or stronger threats on the board.
    Player opponent = (myColor == Player::Black ? Player::White : Player::Black);
    auto myEval = evaluatePlayer(board, myColor);
    auto oppEval = evaluatePlayer(board, opponent);

    const int DOUBLE_OPEN_FOUR_SCORE = 90000000;
    if (myEval.hasOpenFourDouble && !oppEval.hasOpenFourDouble) {
        return DOUBLE_OPEN_FOUR_SCORE;
    }
    if (oppEval.hasOpenFourDouble && !myEval.hasOpenFourDouble) {
        return -DOUBLE_OPEN_FOUR_SCORE;
    }

    // Encourage goal-oriented play: heavily reward longer contiguous lines and
    // open-ended runs, which directly correlate with the ability to win or
    // force the opponent to respond.  The cubic scaling for longestRun makes
    // four-in-a-row far more valuable than scattered stones, nudging the
    // engine toward extending its best chains or cutting the opponent's best.
    auto longestBias = [](const EvalResult &r) {
        int runScore = r.longestRun * r.longestRun * r.longestRun * 500;
        int opennessBonus = r.longestOpenEnds * 20000;
        return runScore + opennessBonus;
    };

    int positionalScore = myEval.patternScore - oppEval.patternScore;
    int shapeScore = longestBias(myEval) - longestBias(oppEval);
    return positionalScore + shapeScore;
}

bool SearchEngine::isWinningMove(const Board &board, Player player, int x, int y) const {
    if (board.isOccupied(x, y)) return false;
    auto cellOf = [&](int cx, int cy) {
        int state = board.getCellState(cx, cy);
        if (state == 0) return 0;
        return (state == 1 ? Player::Black : Player::White) == player ? 1 : -1;
    };
    const int dirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    for (auto &d : dirs) {
        int dx = d[0];
        int dy = d[1];
        int count = 1; // include hypothetical stone at (x,y)
        int nx = x + dx, ny = y + dy;
        while (nx >= 0 && nx < 12 && ny >= 0 && ny < 12 && cellOf(nx, ny) == 1) {
            ++count; nx += dx; ny += dy;
        }
        nx = x - dx; ny = y - dy;
        while (nx >= 0 && nx < 12 && ny >= 0 && ny < 12 && cellOf(nx, ny) == 1) {
            ++count; nx -= dx; ny -= dy;
        }
        if (count >= 5) return true;
    }
    return false;
}

int SearchEngine::alphaBeta(Board &board, int depth, int alpha, int beta,
                  Player currentPlayer, Player myColor, int ply) {
    // This function implements a classic alpha–beta search with a
    // transposition table and various move ordering heuristics.  It
    // returns a score from the perspective of myColor.  The parameters
    // alpha and beta store the best scores found so far along the path
    // and allow pruning: if the current node's score is worse than the
    // existing alpha/beta window, further exploration can be skipped.
    // The parameter currentPlayer determines whose turn it is to move,
    // while ply is the depth from the root and is used for killer moves.

    // Check for time expiration early.
    if (timeUp()) {
        return 0;
    }
    // Check for immediate wins.  If myColor has five in a row, return a large
    // positive value.  If the opponent has five in a row, return a large
    // negative value.  We use a depth penalty to prefer shorter wins and
    // longer losses.
    if (board.checkWin(myColor)) {
        return 100000000 - (maxDepthReached - depth);
    }
    Player opponent = (myColor == Player::Black ? Player::White : Player::Black);
    if (board.checkWin(opponent)) {
        return -100000000 + (maxDepthReached - depth);
    }
    // Depth limit or terminal evaluation.
    if (depth <= 0) {
        return evaluate(board, myColor);
    }

    // Look up this position in the transposition table.
    uint64_t key = board.getHashKey();
    auto it = transTable.find(key);
    if (it != transTable.end()) {
        const TTEntry &entry = it->second;
        // Only use the entry if it was searched to at least the same depth.
        if (entry.depth >= depth) {
            if (entry.flag == 0) {
                return entry.score;
            } else if (entry.flag == 1) {
                // Lower bound: value >= entry.score
                if (entry.score > alpha) alpha = entry.score;
            } else if (entry.flag == 2) {
                // Upper bound: value <= entry.score
                if (entry.score < beta) beta = entry.score;
            }
            if (alpha >= beta) {
                return entry.score;
            }
        }
    }
    // Generate candidate moves.  If there are none, evaluate the position.
    auto candidateMoves = board.getCandidateMoves();
    if (candidateMoves.empty()) {
        return evaluate(board, myColor);
    }
    // Order the moves using heuristics to improve pruning.  Pass the
    // current ply so killer moves at this depth can be prioritized.
    auto ordered = orderMoves(board, currentPlayer, myColor, ply);
    // Keep track of the best value and best move found at this node.
    Move bestMove(-1, -1);
    int bestValue;
    int alphaOrig = alpha;
    int betaOrig = beta;
    if (currentPlayer == myColor) {
        bestValue = std::numeric_limits<int>::min();
        for (const auto &m : ordered) {
            if (timeUp()) break;
            board.makeMove(m.x, m.y);
            int val = alphaBeta(board, depth - 1, alpha, beta,
                                (currentPlayer == Player::Black ? Player::White : Player::Black), myColor, ply + 1);
            board.unmakeMove(m.x, m.y);
            if (timeUp()) {
                return 0;
            }
            if (val > bestValue) {
                bestValue = val;
                bestMove = m;
            }
            if (bestValue > alpha) {
                alpha = bestValue;
            }
            if (alpha >= beta) {
                // Beta cutoff: record killer move and update history heuristic.
                if (ply < MAX_PLY) {
                    if (!(killerMoves[ply][0].x == m.x && killerMoves[ply][0].y == m.y)) {
                        // Shift existing killer move to second slot.
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = m;
                    }
                }
                // Increase history heuristic for this move.  Deeper cutoffs get
                // a larger increment (depth squared).
                history.increment(m, depth);
                break;
            }
        }
    } else {
        bestValue = std::numeric_limits<int>::max();
        for (const auto &m : ordered) {
            if (timeUp()) break;
            board.makeMove(m.x, m.y);
            int val = alphaBeta(board, depth - 1, alpha, beta,
                                (currentPlayer == Player::Black ? Player::White : Player::Black), myColor, ply + 1);
            board.unmakeMove(m.x, m.y);
            if (timeUp()) {
                return 0;
            }
            if (val < bestValue) {
                bestValue = val;
                bestMove = m;
            }
            if (bestValue < beta) {
                beta = bestValue;
            }
            if (alpha >= beta) {
                // Alpha cutoff: record killer move and update history heuristic.
                if (ply < MAX_PLY) {
                    if (!(killerMoves[ply][0].x == m.x && killerMoves[ply][0].y == m.y)) {
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = m;
                    }
                }
                history.increment(m, depth);
                break;
            }
        }
    }
    // Store the result in the transposition table.
    int flag;
    if (bestValue <= alphaOrig) {
        // Fails high: an upper bound.
        flag = 2;
    } else if (bestValue >= betaOrig) {
        // Fails low: a lower bound.
        flag = 1;
    } else {
        // Exact value.
        flag = 0;
    }
    TTEntry newEntry;
    newEntry.depth = depth;
    newEntry.score = bestValue;
    newEntry.flag = flag;
    newEntry.bestMove = bestMove;
    transTable[key] = newEntry;
    return bestValue;
}

Move SearchEngine::findBestMove(Board &board, Player myColor, int timeLimitMs) {
    // Set up the timer for this move.  The search will stop when
    // timeUp() becomes true.
    startTimer(timeLimitMs);
    // Clear the transposition table at the start of each search.  Using
    // a fresh table prevents reuse of stale entries from previous moves
    // and bounds the memory footprint.
    transTable.clear();
    // Reset the history heuristic table for this search.  History values
    // accumulate within a single search but are cleared between moves.
    history.reset();
    // Reset killer moves.  Mark all moves as invalid (-1,-1).
    for (int i = 0; i < MAX_PLY; ++i) {
        killerMoves[i][0] = Move(-1, -1);
        killerMoves[i][1] = Move(-1, -1);
    }
    // Opening book: if we are in the predetermined opening and it is
    // our turn to move as Black, select a hard‑coded central move.
    Move bookMove;
    if (getOpeningMove(board, myColor, bookMove)) {
        return bookMove;
    }

    // 1) Tactical override: if we can win immediately, do so without further search.
    auto legalMoves = board.getLegalMoves();
    for (const auto &m : legalMoves) {
        if (isWinningMove(board, myColor, m.x, m.y)) {
            return m;
        }
    }

    // 2) Urgent defense: if the opponent has a winning move next turn, block it
    // if possible. If multiple blocking squares exist, pick the one that keeps
    // the best evaluation after our placement.
    Player opponent = (myColor == Player::Black ? Player::White : Player::Black);
    std::vector<Move> opponentWins;
    for (const auto &m : legalMoves) {
        if (isWinningMove(board, opponent, m.x, m.y)) {
            opponentWins.push_back(m);
        }
    }
    if (!opponentWins.empty()) {
        Move bestBlock = opponentWins.front();
        int bestScore = std::numeric_limits<int>::min();
        for (const auto &block : opponentWins) {
            board.makeMove(block.x, block.y);
            int score = evaluate(board, myColor);
            board.unmakeMove(block.x, block.y);
            if (score > bestScore) {
                bestScore = score;
                bestBlock = block;
            }
        }
        return bestBlock;
    }

    // Urgent defensive move: if the opponent has an immediate tactical threat
    // (e.g., open four or a highly flexible three), answer it before starting
    // the full search to avoid time-consuming but obvious defenses.
    auto defensiveMoves = threatSolver.findBlockingMoves(board, myColor);
    if (!defensiveMoves.empty()) {
        const int CRITICAL_SEVERITY = 500000; // open fours and simple fours.
        if (defensiveMoves.front().severity >= CRITICAL_SEVERITY) {
            return defensiveMoves.front().move;
        }
    }
    Move bestMove(-1, -1);
    // Generate and order root moves.  These moves will be re‑ordered
    // between iterations based on the values returned by the search.
    auto rootMoves = orderMoves(board, myColor, myColor, 0);
    if (rootMoves.empty()) {
        return bestMove;
    }
    int bestVal = std::numeric_limits<int>::min();
    // Begin iterative deepening: increase the search depth one ply at a time.
    for (int depth = 1; ; ++depth) {
        if (timeUp()) break;
        maxDepthReached = depth;
        Move currentBestMove = rootMoves.front();
        int currentBestVal = std::numeric_limits<int>::min();
        for (const auto &m : rootMoves) {
            if (timeUp()) break;
            board.makeMove(m.x, m.y);
            int val = alphaBeta(board, depth - 1,
                                std::numeric_limits<int>::min() + 1,
                                std::numeric_limits<int>::max() - 1,
                                (myColor == Player::Black ? Player::White : Player::Black),
                                myColor,
                                1);
            board.unmakeMove(m.x, m.y);
            if (timeUp()) break;
            // If the returned value indicates a certain win (large positive),
            // we can return this move immediately.
            if (val > 90000000) {
                return m;
            }
            if (val > currentBestVal) {
                currentBestVal = val;
                currentBestMove = m;
            }
        }
        if (!timeUp()) {
            bestVal = currentBestVal;
            bestMove = currentBestMove;
        // Reorder root moves based on their values for the next iteration.
        // Moves that scored better are tried first in deeper searches.
            std::vector<std::pair<int, Move>> scored;
            scored.reserve(rootMoves.size());
            for (const auto &m : rootMoves) {
                int val;
                board.makeMove(m.x, m.y);
                val = alphaBeta(board, depth - 1,
                                std::numeric_limits<int>::min() + 1,
                                std::numeric_limits<int>::max() - 1,
                                (myColor == Player::Black ? Player::White : Player::Black),
                                myColor,
                                1);
                board.unmakeMove(m.x, m.y);
                scored.push_back({val, m});
            }
            std::sort(scored.begin(), scored.end(), [](const std::pair<int, Move> &a, const std::pair<int, Move> &b) {
                return a.first > b.first;
            });
            rootMoves.clear();
            for (auto &p : scored) rootMoves.push_back(p.second);
        } else {
            break;
        }
    }
    return bestMove;
}

std::vector<Move> SearchEngine::orderMoves(Board &board, Player currentPlayer, Player myColor, int ply) {
    // Generate candidate moves within the board's bounding box and near
    // existing stones.  These moves form the basis for move ordering.
    auto moves = board.getCandidateMoves();
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    Player opponent = (currentPlayer == Player::Black ? Player::White : Player::Black);

    // Surface urgent defensive moves against the opponent's most dangerous
    // threats (e.g., open fours or open/broken threes) so they are explored
    // early.
    auto defensiveMoves = threatSolver.findBlockingMoves(board, currentPlayer);
    std::unordered_map<int, int> defensiveLookup;
    for (const auto &t : defensiveMoves) {
        int key = t.move.y * 12 + t.move.x;
        if (defensiveLookup.find(key) == defensiveLookup.end() || defensiveLookup[key] < t.severity) {
            defensiveLookup[key] = t.severity;
        }
    }
    for (const auto &m : moves) {
        int score = 0;
        // Make the move and evaluate consequences.
        board.makeMove(m.x, m.y);
        // Check for immediate win for the player who plays this move.
        bool winForCurrent = board.checkWin(currentPlayer);
        // Check if this move would allow the opponent to win on their next turn.
        bool winForOpp = false;
        auto oppCandidates = board.getCandidateMoves();
        for (const auto &oppMove : oppCandidates) {
            if (isWinningMove(board, opponent, oppMove.x, oppMove.y)) {
                winForOpp = true;
                break;
            }
        }
        // Evaluate board from myColor perspective; larger is better for myColor.
        int evalScore = evaluate(board, myColor);
        board.unmakeMove(m.x, m.y);
        // Scoring heuristic:
        //  * If the move wins immediately for the current player, assign a
        //    very large score to ensure it is tried first.
        if (winForCurrent) {
            score = 100000000;
        } else if (winForOpp) {
            // If the move inadvertently allows the opponent to win, penalize
            // heavily to avoid self‑destruction.
            score = -10000000;
        } else {
            score = evalScore;
        }
        // Additional heuristic: prefer moves closer to the center.
        int dx = m.x - 5;
        int dy = m.y - 5;
        score -= (dx * dx + dy * dy);
        // Prioritize blocking high-severity opponent threats.
        int key = m.y * 12 + m.x;
        auto defIt = defensiveLookup.find(key);
        if (defIt != defensiveLookup.end()) {
            score += defIt->second;
        }
        // Killer move heuristic: if this move is one of the recorded killer moves
        // at the current search ply, add a large bonus.  The first killer move
        // receives a larger bonus than the second.  Killer moves are ones that
        // previously caused a beta or alpha cutoff at this depth and are good
        // candidates for pruning.
        const int KILLER_BONUS1 = 1000000;
        const int KILLER_BONUS2 = 500000;
        if (ply < MAX_PLY) {
            if (killerMoves[ply][0].x == m.x && killerMoves[ply][0].y == m.y) {
                score += KILLER_BONUS1;
            } else if (killerMoves[ply][1].x == m.x && killerMoves[ply][1].y == m.y) {
                score += KILLER_BONUS2;
            }
        }
        // History heuristic: add the history table value for this move.
        // Moves that have frequently caused cutoffs in this search will be
        // tried earlier.  Deeper cutoffs contribute more to the history
        // table because increment() adds depth².
        score += history.get(m);
        scored.push_back({score, m});
    }
    // Sort descending by heuristic score so the best candidates appear first.
    std::sort(scored.begin(), scored.end(), [](const std::pair<int, Move> &a, const std::pair<int, Move> &b) {
        return a.first > b.first;
    });
    std::vector<Move> ordered;
    ordered.reserve(scored.size());
    for (auto &p : scored) {
        ordered.push_back(p.second);
    }
    return ordered;
}

} // namespace gomoku