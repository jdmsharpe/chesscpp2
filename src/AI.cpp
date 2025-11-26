#include "AI.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <fstream>
#include <sstream>
#include <random>

#include "Bitboard.h"
#include "Logger.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Tablebase.h"

AI::AI(int depth)
    : depth(depth),
      nodesSearched(0),
      ttHits(0),
      timeLimit(0),
      searchStartTime(0),
      stopSearch(false),
      moveCallback(nullptr),
      transpositionTable(TT_SIZE),
      ttAge(0),
      killerMoves{},
      historyTable{},
      pvTable{},
      pvLength{},
      countermoves{} {
  std::ostringstream oss;
  oss << "AI::AI() constructor called (this=" << this << ")";
  Logger::getInstance().debug(oss.str());
}

// Helper to get current time in milliseconds
static uint64_t currentTimeMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

bool AI::shouldStop() const {
  if (timeLimit == 0) return false;
  uint64_t elapsed = currentTimeMs() - searchStartTime;
  return elapsed >= static_cast<uint64_t>(timeLimit);
}

// Opening book implementation
void AI::loadOpeningBook(const std::string& filename) {
  openingBook.clear();
  std::ifstream file(filename);
  if (!file.is_open()) {
    Logger::getInstance().warning("Could not open opening book: " + filename);
    return;
  }

  std::string line;
  int lineCount = 0;
  while (std::getline(file, line)) {
    lineCount++;
    // Skip comments and empty lines
    if (line.empty() || line[0] == '#') continue;

    // Parse line: FEN | move1 move2 move3
    size_t pipePos = line.find('|');
    if (pipePos == std::string::npos) continue;

    std::string fen = line.substr(0, pipePos);
    std::string movesStr = line.substr(pipePos + 1);

    // Trim whitespace from FEN
    fen.erase(0, fen.find_first_not_of(" \t"));
    fen.erase(fen.find_last_not_of(" \t") + 1);

    // Parse moves
    std::istringstream iss(movesStr);
    std::string moveStr;
    std::vector<Move> moves;

    // Create position from FEN to validate moves
    Position bookPos;
    if (!bookPos.setFromFEN(fen)) {
      std::cerr << "Warning: Invalid FEN in book line " << lineCount << ": " << fen << std::endl;
      continue;
    }

    while (iss >> moveStr) {
      // Parse move in UCI format (e.g., "e2e4")
      if (moveStr.length() < 4) continue;

      Square from = Square((moveStr[1] - '1') * 8 + (moveStr[0] - 'a'));
      Square to = Square((moveStr[3] - '1') * 8 + (moveStr[2] - 'a'));

      // Check for promotion
      PieceType promotion = NO_PIECE_TYPE;
      if (moveStr.length() == 5) {
        char promChar = moveStr[4];
        if (promChar == 'q') promotion = QUEEN;
        else if (promChar == 'r') promotion = ROOK;
        else if (promChar == 'b') promotion = BISHOP;
        else if (promChar == 'n') promotion = KNIGHT;
      }

      // Generate legal moves to find the matching move
      std::vector<Move> legalMoves = MoveGen::generateLegalMoves(bookPos);

      for (Move m : legalMoves) {
        if (fromSquare(m) == from && toSquare(m) == to) {
          if (promotion == NO_PIECE_TYPE || promotionType(m) == promotion) {
            moves.push_back(m);
            break;
          }
        }
      }
    }

    if (!moves.empty()) {
      openingBook[fen] = moves;
    }
  }

  std::ostringstream oss;
  oss << "Loaded opening book with " << openingBook.size() << " positions (AI at " << this << ")";
  Logger::getInstance().info(oss.str());
}

Move AI::probeOpeningBook(const Position& pos) {
  std::ostringstream oss;
  oss << "Probing: AI at " << this << ", book size: " << openingBook.size();
  Logger::getInstance().debug(oss.str());

  if (openingBook.empty()) return 0;

  std::string fen = pos.getFEN();
  auto it = openingBook.find(fen);
  if (it == openingBook.end()) return 0;

  const std::vector<Move>& bookMoves = it->second;
  if (bookMoves.empty()) return 0;

  // Randomly select from the first few moves (add some variety)
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // Prefer earlier moves but sometimes play later ones
  int maxIndex = std::min(3, static_cast<int>(bookMoves.size()));
  std::uniform_int_distribution<> dis(0, maxIndex - 1);

  return bookMoves[dis(gen)];
}

bool AI::hasOpeningBook() const {
  return !openingBook.empty();
}

Move AI::findBestMove(Position& pos, int timeMs) {
  timeLimit = timeMs;
  searchStartTime = currentTimeMs();
  stopSearch = false;
  return findBestMove(pos);
}

Move AI::findBestMove(Position& pos) {
  // Check opening book first
  Move bookMove = probeOpeningBook(pos);
  if (bookMove != 0) {
    // UCI-compliant logging
    std::cout << "info string Book move: " << moveToString(bookMove) << std::endl;
    return bookMove;
  }

  // Check Syzygy tablebases at root
  if (Tablebase::available() && Tablebase::canProbe(pos)) {
    TBProbeResult tbResult = Tablebase::probeRoot(pos);
    if (tbResult.success && tbResult.bestMove != 0) {
      const char* wdlStr = "unknown";
      switch (tbResult.wdl) {
        case TB_RESULT_WIN: wdlStr = "win"; break;
        case TB_RESULT_CURSED_WIN: wdlStr = "cursed win"; break;
        case TB_RESULT_DRAW: wdlStr = "draw"; break;
        case TB_RESULT_BLESSED_LOSS: wdlStr = "blessed loss"; break;
        case TB_RESULT_LOSS: wdlStr = "loss"; break;
        default: break;
      }
      std::cout << "info string Tablebase hit: " << wdlStr
                << " (DTZ: " << tbResult.dtz << ")" << std::endl;
      return tbResult.bestMove;
    }
  }

  std::cout << "info string Searching (no book move)..." << std::endl;
  nodesSearched = 0;
  ttHits = 0;
  ttAge++;  // Increment age for new search

  std::vector<Move> rootMoves = MoveGen::generateLegalMoves(pos);
  if (rootMoves.empty()) return 0;

  Move bestMove = rootMoves[0];
  int bestScore = std::numeric_limits<int>::min();

  std::cout << "Using iterative deepening:\n";

  // Iterative deepening
  for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
    // Check time limit before starting new depth
    if (shouldStop()) {
      std::cout << "  Time limit reached, stopping at depth "
                << (currentDepth - 1) << "\n";
      break;
    }

    // Aspiration windows - start narrow, widen if needed
    int alpha, beta;
    if (currentDepth >= 5 && bestScore != std::numeric_limits<int>::min()) {
      // Use aspiration window based on previous score
      const int ASPIRATION_WINDOW = 50;
      alpha = bestScore - ASPIRATION_WINDOW;
      beta = bestScore + ASPIRATION_WINDOW;
    } else {
      // Full window for shallow depths
      alpha = std::numeric_limits<int>::min() + 1;
      beta = std::numeric_limits<int>::max() - 1;
    }

    // Order moves based on previous iteration's best move
    orderMoves(pos, rootMoves, 0, bestMove);

    Move iterBestMove = rootMoves[0];
    int iterBestScore = std::numeric_limits<int>::min();

    std::cout << "  Depth " << currentDepth << ": ";
    std::cout.flush();

    for (Move move : rootMoves) {
      // Show move being searched
      std::cout << moveToString(move) << " ";
      std::cout.flush();

      // Call GUI callback if set
      if (moveCallback) {
        moveCallback(move, currentDepth, pos);
      }

      pos.makeMove(move);
      int score = -negamax(pos, currentDepth - 1, -beta, -alpha, 1);
      pos.unmakeMove();

      if (score > iterBestScore) {
        iterBestScore = score;
        iterBestMove = move;
      }

      alpha = std::max(alpha, score);
    }

    // Check if we need to re-search with wider window
    if (currentDepth >= 5 &&
        (iterBestScore <= alpha - 50 || iterBestScore >= beta + 50)) {
      // Failed outside aspiration window - re-search only the best move with
      // full window
      std::cout << " [re-search " << moveToString(iterBestMove) << "]";
      alpha = std::numeric_limits<int>::min() + 1;
      beta = std::numeric_limits<int>::max() - 1;

      pos.makeMove(iterBestMove);
      iterBestScore = -negamax(pos, currentDepth - 1, -beta, -alpha, 1);
      pos.unmakeMove();
    }

    bestMove = iterBestMove;
    bestScore = iterBestScore;

    std::cout << "\n  Depth " << currentDepth
              << " complete: " << moveToString(bestMove)
              << " (score: " << bestScore << ", nodes: " << nodesSearched
              << ", tt hits: " << ttHits << ")\n";
  }

  std::cout << "Best move: " << moveToString(bestMove)
            << " (score: " << bestScore << ")\n";
  std::cout << "Total nodes: " << nodesSearched << ", TT hits: " << ttHits
            << "\n";

  return bestMove;
}

int AI::negamax(Position& pos, int depth, int alpha, int beta, int ply) {
  nodesSearched++;

  // Periodic time check (every 1024 nodes)
  if ((nodesSearched & 0x3FF) == 0 && shouldStop()) {
    return 0;  // Return immediately if time is up
  }

  int alphaOrig = alpha;
  HashKey hash = pos.hash();

  // Transposition table lookup
  size_t ttIndex = hash % TT_SIZE;
  TTEntry& ttEntry = transpositionTable[ttIndex];

  if (ttEntry.key == hash && ttEntry.depth >= depth) {
    ttHits++;
    if (ttEntry.flag == EXACT) {
      return ttEntry.score;
    } else if (ttEntry.flag == LOWERBOUND) {
      alpha = std::max(alpha, ttEntry.score);
    } else if (ttEntry.flag == UPPERBOUND) {
      beta = std::min(beta, ttEntry.score);
    }

    if (alpha >= beta) {
      return ttEntry.score;
    }
  }

  // Quiescence search at leaf nodes
  if (depth == 0) {
    pvLength[ply] = 0;  // No PV in qsearch
    return quiescence(pos, alpha, beta, 0);
  }

  // Null Move Pruning
  // Try "passing" - if position is still good, we can prune
  const int NULL_MOVE_REDUCTION = 3;
  bool canDoNullMove = depth >= 3 && !pos.inCheck() && ply > 0;

  // Avoid null move in zugzwang-prone positions
  if (canDoNullMove) {
    Color us = pos.sideToMove();
    int material = pos.materialCount(us);

    // Count non-pawn pieces
    int knights = BB::popCount(pos.pieces(us, KNIGHT));
    int bishops = BB::popCount(pos.pieces(us, BISHOP));
    int rooks = BB::popCount(pos.pieces(us, ROOK));
    int queens = BB::popCount(pos.pieces(us, QUEEN));
    int nonPawnPieces = knights + bishops + rooks + queens;

    // Skip null move if:
    // 1. Only king + pawns (zugzwang prone)
    // 2. Pawn endgames (zugzwang common)
    // 3. Very little material (K+minor vs K endgames)
    if (material <= 100 ||                         // Only pawns
        nonPawnPieces == 0 ||                      // No pieces, only pawns
        (nonPawnPieces == 1 && material < 500)) {  // Only one minor/rook
      canDoNullMove = false;
    }
  }

  if (canDoNullMove) {
    pos.makeNullMove();
    int nullDepth = std::max(0, depth - 1 - NULL_MOVE_REDUCTION);
    int score = -negamax(pos, nullDepth, -beta, -beta + 1, ply + 1);
    pos.unmakeNullMove();

    if (score >= beta) {
      return beta;  // Null move caused cutoff
    }
  }

  // Determine if this is a PV node (wide window = principal variation)
  bool isPVNode = (beta - alpha) > 1;

  // Reverse Futility Pruning (Static Null Move Pruning)
  // If position is so good that even with a margin we're above beta, prune
  if (depth <= 6 && !isPVNode && !pos.inCheck()) {
    int eval = evaluate(pos);
    int rfpMargin = 100 * depth;  // 100, 200, 300... 600 for depths 1-6
    if (eval - rfpMargin >= beta) {
      return eval;  // Position too good, opponent won't allow this line
    }
  }

  // Razoring
  // If position is so bad that even qsearch can't save it, return qsearch score
  if (depth <= 3 && !isPVNode && !pos.inCheck()) {
    int eval = evaluate(pos);
    int razoringMargin = 300 + 150 * depth;  // 450, 600, 750 for depths 1-3
    if (eval + razoringMargin < alpha) {
      int qscore = quiescence(pos, alpha, beta, 0);
      if (qscore < alpha) {
        return qscore;
      }
    }
  }

  // Futility Pruning
  // At shallow depths, if position is so bad that even gaining material won't
  // help, skip quiet moves
  bool futilityPrune = false;
  if (depth <= 3 && !pos.inCheck()) {
    int futilityMargin = 100 + 200 * depth;  // 300, 500, 700 for depths 1-3
    int futilityValue = evaluate(pos) + futilityMargin;

    if (futilityValue <= alpha) {
      futilityPrune = true;
    }
  }

  std::vector<Move> moves = MoveGen::generateLegalMoves(pos);

  // Checkmate or stalemate
  if (moves.empty()) {
    if (pos.inCheck()) {
      // Checkmate - return low score (prefer shorter mates)
      return -10000 + ply;
    } else {
      // Stalemate
      return 0;
    }
  }

  // Move ordering - get TT move if available
  Move ttMove = 0;
  if (ttEntry.key == hash) {
    ttMove = ttEntry.bestMove;
  }

  // Internal Iterative Deepening (IID)
  // If no hash move at PV node, do shallow search to find one
  if (!ttMove && isPVNode && depth >= 4) {
    int iidDepth = depth - 2;
    // Do shallow search to populate TT with a good move
    (void)negamax(pos, iidDepth, alpha, beta, ply);

    // Try to get the move from TT after shallow search
    TTEntry& iidEntry = transpositionTable[ttIndex];
    if (iidEntry.key == hash && iidEntry.bestMove != 0) {
      ttMove = iidEntry.bestMove;
      // Validate the move is in our move list
      bool found = false;
      for (Move m : moves) {
        if (m == ttMove) {
          found = true;
          break;
        }
      }
      if (!found) ttMove = 0;  // Invalid move, ignore
    }
  }

  orderMoves(pos, moves, ply, ttMove);

  int maxScore = std::numeric_limits<int>::min();
  Move bestMove = moves[0];
  pvLength[ply] = 0;  // Initialize PV length

  for (size_t moveNum = 0; moveNum < moves.size(); ++moveNum) {
    Move move = moves[moveNum];

    bool isCapture = pos.pieceAt(toSquare(move)) != NO_PIECE ||
                     moveType(move) == EN_PASSANT;
    bool isPromotion = moveType(move) == PROMOTION;

    // Apply futility pruning: skip quiet moves if position is hopeless
    if (futilityPrune && moveNum > 0) {
      // Skip quiet moves (non-captures, non-promotions)
      if (!isCapture && !isPromotion) {
        continue;
      }
    }

    // Late Move Pruning - skip late quiet moves entirely at low depths
    if (depth <= 3 && moveNum >= static_cast<size_t>(3 + depth * depth) &&
        !isCapture && !isPromotion && !isKiller(move, ply)) {
      continue;  // Prune this move entirely
    }

    pos.makeMove(move);

    // Check Extension - extend search when giving check
    bool givesCheck = pos.inCheck();
    int extension = 0;
    if (givesCheck) {
      extension = 1;  // Search one ply deeper when checking
    }

    int score;
    int newDepth = depth - 1 + extension;

    // Principal Variation Search (PVS)
    if (moveNum == 0) {
      // First move - search with full window
      score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
    } else {
      // Late Move Reductions (LMR)
      int reduction = 0;

      // Don't reduce checking moves, captures, or promotions
      if (depth >= 3 && moveNum >= 3 && !isCapture && !givesCheck &&
          !isPromotion && !isKiller(move, ply)) {
        // More aggressive reduction formula: log(depth) * log(moveNum)
        reduction = 1;
        if (depth >= 3 && moveNum >= 3) {
          // Approximate log reduction: reduction increases with depth and move
          // number
          reduction = 1 + (depth >= 6 ? 1 : 0) + (moveNum >= 6 ? 1 : 0);
          if (depth >= 8 && moveNum >= 10) {
            reduction++;  // Extra reduction for very deep/late moves
          }
        }
        // Clamp reduction to prevent over-reduction
        reduction = std::min(reduction, newDepth);
      }

      // PVS: Try null window search first
      score = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1);

      // If null window search fails high, do full re-search
      if (score > alpha && score < beta) {
        // Re-search at full depth if we reduced
        if (reduction > 0) {
          score = -negamax(pos, newDepth, -alpha - 1, -alpha, ply + 1);
        }

        // Full window re-search if still beats alpha
        if (score > alpha && score < beta) {
          score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
        }
      }
    }

    pos.unmakeMove();

    if (score > maxScore) {
      maxScore = score;
      bestMove = move;

      // Update PV
      pvTable[ply][0] = move;
      for (int i = 0; i < pvLength[ply + 1]; ++i) {
        pvTable[ply][i + 1] = pvTable[ply + 1][i];
      }
      pvLength[ply] = pvLength[ply + 1] + 1;
    }

    alpha = std::max(alpha, score);

    if (alpha >= beta) {
      // Beta cutoff - store killer move and countermove if not a capture
      bool isCapture = pos.pieceAt(toSquare(move)) != NO_PIECE ||
                       moveType(move) == EN_PASSANT;
      if (!isCapture) {
        storeKiller(move, ply);
        updateHistory(move, depth);

        // Store countermove heuristic
        // This move refuted the previous move
        if (ply > 0 && pvLength[ply - 1] > 0) {
          Move prevMove = pvTable[ply - 1][0];
          countermoves[fromSquare(prevMove)][toSquare(prevMove)] = move;
        }
      }
      break;
    }
  }

  // Store in transposition table with depth-preferred replacement
  TTEntry& entry = transpositionTable[ttIndex];

  // Replace if: empty slot, same position, deeper search, or older entry
  bool shouldReplace = (entry.key == 0) ||        // Empty slot
                       (entry.key == hash) ||     // Same position
                       (entry.depth <= depth) ||  // Deeper search
                       (entry.age != ttAge);      // Old entry

  if (shouldReplace) {
    entry.key = hash;
    entry.depth = depth;
    entry.score = maxScore;
    entry.bestMove = bestMove;
    entry.age = ttAge;

    if (maxScore <= alphaOrig) {
      entry.flag = UPPERBOUND;
    } else if (maxScore >= beta) {
      entry.flag = LOWERBOUND;
    } else {
      entry.flag = EXACT;
    }
  }

  return maxScore;
}

int AI::quiescence(Position& pos, int alpha, int beta, int qsDepth) {
  nodesSearched++;

  // Check extension: if in check, search deeper
  bool inCheck = pos.inCheck();

  // Stand pat score (not valid if in check - must move)
  int standPat = 0;
  if (!inCheck) {
    standPat = evaluate(pos);

    if (standPat >= beta) {
      return beta;
    }

    // Delta pruning margin (queen value)
    constexpr int DELTA_MARGIN = 900;

    // Delta pruning - if we're too far behind, even capturing a queen won't
    // help
    if (standPat + DELTA_MARGIN < alpha) {
      return alpha;
    }

    if (alpha < standPat) {
      alpha = standPat;
    }
  }

  // Generate captures (and checks at qdepth 0)
  std::vector<Move> captures = MoveGen::generateCaptures(pos);

  // At first qsearch ply, also try checking moves to avoid horizon effect
  std::vector<Move> checks;
  if (qsDepth == 0 && !inCheck) {
    std::vector<Move> allMoves = MoveGen::generateLegalMoves(pos);
    for (Move m : allMoves) {
      // Skip if already a capture
      bool isCapture =
          pos.pieceAt(toSquare(m)) != NO_PIECE || moveType(m) == EN_PASSANT;
      if (!isCapture) {
        // Check if move gives check
        pos.makeMove(m);
        bool givesCheck = pos.inCheck();
        pos.unmakeMove();
        if (givesCheck) {
          checks.push_back(m);
        }
      }
    }
    // Merge checks into captures list
    captures.insert(captures.end(), checks.begin(), checks.end());
  }

  // Order captures by SEE
  std::sort(captures.begin(), captures.end(), [&](Move a, Move b) {
    return getMoveScore(pos, a, 0, 0) > getMoveScore(pos, b, 0, 0);
  });

  for (Move move : captures) {
    // SEE pruning - skip obviously losing captures
    int seeValue = pos.see(move);
    if (seeValue < 0) {
      continue;  // Skip bad captures
    }

    // Futility pruning - even if this capture succeeds, can it improve alpha?
    Square to = toSquare(move);
    Piece captured = pos.pieceAt(to);
    if (captured != NO_PIECE) {
      static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
      if (standPat + pieceValues[typeOf(captured)] + 200 < alpha) {
        continue;  // Even capturing this piece won't help
      }
    }

    pos.makeMove(move);
    int score = -quiescence(pos, -beta, -alpha, qsDepth + 1);
    pos.unmakeMove();

    if (score >= beta) {
      return beta;
    }

    if (score > alpha) {
      alpha = score;
    }
  }

  return alpha;
}

// Piece-square tables for positional evaluation (constexpr for compile-time
// init)
namespace PST {
// Pawns - STRONGLY encourage center control and advancement
constexpr int pawn[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,    // Rank 1
    5,   10,  10, -20, -20,  10,  10,  5,    // Rank 2 (discourage early edge pawns)
    5,   10,  20,  40,  40,  20,  10,  5,    // Rank 3 (increased center)
    10,  15,  30,  70,  70,  30,  15,  10,   // Rank 4 (MAJOR bonus for e4/d4!)
    15,  20,  35,  80,  80,  35,  20,  15,   // Rank 5 (advanced center - huge bonus)
    20,  25,  30,  35,  35,  30,  25,  20,   // Rank 6
    50,  50,  50,  50,  50,  50,  50,  50,   // Rank 7 (about to promote)
    0,   0,   0,   0,   0,   0,   0,   0};   // Rank 8

// Knights - heavily prefer center, punish edges
constexpr int knight[64] = {
    -50, -40, -30, -25, -25, -30, -40, -50,  // Rank 1 (bad)
    -40, -20,   0,   5,   5,   0, -20, -40,  // Rank 2
    -30,   5,  10,  15,  15,  10,   5, -30,  // Rank 3
    -25,   5,  15,  20,  20,  15,   5, -25,  // Rank 4
    -25,   5,  15,  20,  20,  15,   5, -25,  // Rank 5
    -30,   5,  10,  15,  15,  10,   5, -30,  // Rank 6
    -40, -20,   0,   5,   5,   0, -20, -40,  // Rank 7
    -50, -40, -30, -25, -25, -30, -40, -50}; // Rank 8 (very bad)

// Bishops - prefer long diagonals and center
constexpr int bishop[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,  // Rank 1
    -10,   5,   0,   0,   0,   0,   5, -10,  // Rank 2
    -10,  10,  10,  10,  10,  10,  10, -10,  // Rank 3
    -10,   0,  10,  15,  15,  10,   0, -10,  // Rank 4
    -10,   5,   5,  15,  15,   5,   5, -10,  // Rank 5
    -10,   0,   5,  10,  10,   5,   0, -10,  // Rank 6
    -10,   5,   0,   0,   0,   0,   5, -10,  // Rank 7
    -20, -10, -10, -10, -10, -10, -10, -20}; // Rank 8

// Rooks - prefer 7th rank and open files (back rank OK)
constexpr int rook[64] = {
     0,   0,   0,   5,   5,   0,   0,   0,   // Rank 1
    20,  20,  20,  20,  20,  20,  20,  20,   // Rank 2 (7th rank for black)
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 3
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 4
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 5
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 6
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 7
     0,   0,   0,   0,   0,   0,   0,   0};  // Rank 8

// Kings - stay safe in middlegame (castled position)
constexpr int kingMiddle[64] = {
     20,  30,  10,   0,   0,  10,  30,  20,  // Rank 1 (castle!)
    -10, -20, -20, -20, -20, -20, -20, -10,  // Rank 2 (don't advance)
    -20, -30, -30, -40, -40, -30, -30, -20,  // Rank 3
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 4
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 5
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 6
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 7
    -30, -40, -40, -50, -50, -40, -40, -30}; // Rank 8
}  // namespace PST

int AI::evaluate(const Position& pos) {
  // Material evaluation
  int material = pos.materialCount(WHITE) - pos.materialCount(BLACK);

  int positional = 0;

  // Evaluate white pieces
  Bitboard pawns = pos.pieces(WHITE, PAWN);
  while (pawns) {
    Square sq = BB::popLsb(pawns);
    positional += PST::pawn[sq];
  }

  Bitboard knights = pos.pieces(WHITE, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    positional += PST::knight[sq];
  }

  Bitboard bishops = pos.pieces(WHITE, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    positional += PST::bishop[sq];
  }

  Bitboard rooks = pos.pieces(WHITE, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    positional += PST::rook[sq];
  }

  Bitboard kings = pos.pieces(WHITE, KING);
  while (kings) {
    Square sq = BB::popLsb(kings);
    positional += PST::kingMiddle[sq];
  }

  // Evaluate black pieces (flip board)
  pawns = pos.pieces(BLACK, PAWN);
  while (pawns) {
    Square sq = BB::popLsb(pawns);
    positional -= PST::pawn[sq ^ 56];  // Flip rank
  }

  knights = pos.pieces(BLACK, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    positional -= PST::knight[sq ^ 56];
  }

  bishops = pos.pieces(BLACK, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    positional -= PST::bishop[sq ^ 56];
  }

  rooks = pos.pieces(BLACK, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    positional -= PST::rook[sq ^ 56];
  }

  kings = pos.pieces(BLACK, KING);
  while (kings) {
    Square sq = BB::popLsb(kings);
    positional -= PST::kingMiddle[sq ^ 56];
  }

  // Add advanced evaluation features
  int pawnStructure =
      evaluatePawnStructure(pos, WHITE) - evaluatePawnStructure(pos, BLACK);
  int kingSafety =
      evaluateKingSafety(pos, WHITE) - evaluateKingSafety(pos, BLACK);
  int mobility = evaluateMobility(pos, WHITE) - evaluateMobility(pos, BLACK);
  int development =
      evaluateDevelopment(pos, WHITE) - evaluateDevelopment(pos, BLACK);
  int rookScore = evaluateRooks(pos, WHITE) - evaluateRooks(pos, BLACK);
  int bishopScore = evaluateBishops(pos, WHITE) - evaluateBishops(pos, BLACK);
  int knightScore = evaluateKnights(pos, WHITE) - evaluateKnights(pos, BLACK);

  // Tapered evaluation - blend opening/endgame scores based on game phase
  int phase = getGamePhase(pos);  // 0 (endgame) to 256 (opening)

  // Opening scores - full weight (development matters most in opening!)
  int openingScore = material + positional + mobility + kingSafety +
                     pawnStructure + development + rookScore + bishopScore +
                     knightScore;

  // Endgame scores - adjust weights
  // In endgame: reduce positional/mobility, reduce king safety, increase pawn
  // structure, ignore development, increase rook value (rooks dominate endgame)
  int endgameScore = material + (positional / 2) + (mobility / 2) +
                     (kingSafety / 4) + (pawnStructure * 3 / 2) +
                     (rookScore * 3 / 2) + bishopScore + knightScore;

  // Interpolate between opening and endgame
  int score = (openingScore * phase + endgameScore * (256 - phase)) / 256;

  // Return from perspective of side to move
  return (pos.sideToMove() == WHITE) ? score : -score;
}

int AI::evaluatePawnStructure(const Position& pos, Color c) const {
  int score = 0;
  Bitboard pawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (pawns) {
    Square sq = BB::popLsb(pawns);
    int file = fileOf(sq);
    int rank = rankOf(sq);

    // Doubled pawns penalty
    Bitboard filePawns = pos.pieces(c, PAWN) & BB::fileBB(file);
    if (BB::popCount(filePawns) > 1) {
      score -= 10;  // Penalty for doubled pawns
    }

    // Isolated pawns penalty
    Bitboard adjacentFiles = 0ULL;
    if (file > 0) adjacentFiles |= BB::fileBB(file - 1);
    if (file < 7) adjacentFiles |= BB::fileBB(file + 1);

    if (!(pos.pieces(c, PAWN) & adjacentFiles)) {
      score -= 15;  // Penalty for isolated pawns
    }

    // Passed pawns bonus
    Bitboard passedMask = 0ULL;
    if (c == WHITE) {
      // For white, check files ahead (higher ranks)
      for (int r = rank + 1; r < 8; ++r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    } else {
      // For black, check files ahead (lower ranks)
      for (int r = rank - 1; r >= 0; --r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    }

    if (!(enemyPawns & passedMask)) {
      // Passed pawn bonus increases with advancement
      int bonus = c == WHITE ? (rank - 1) * 10 : (6 - rank) * 10;
      score += 20 + bonus;
    } else {
      // Check for backward pawns (not passed and cannot be defended by pawns)
      // A pawn is backward if:
      // 1. No friendly pawns on adjacent files can support it (all behind)
      // 2. Enemy pawns control the square in front of it

      bool hasSupport = false;
      Bitboard supportMask = 0ULL;

      // Check for supporting pawns on adjacent files behind this pawn
      if (c == WHITE) {
        for (int r = 0; r <= rank; ++r) {
          if (file > 0) supportMask |= BB::squareBB(r * 8 + file - 1);
          if (file < 7) supportMask |= BB::squareBB(r * 8 + file + 1);
        }
      } else {
        for (int r = rank; r < 8; ++r) {
          if (file > 0) supportMask |= BB::squareBB(r * 8 + file - 1);
          if (file < 7) supportMask |= BB::squareBB(r * 8 + file + 1);
        }
      }

      if (pos.pieces(c, PAWN) & supportMask) {
        hasSupport = true;
      }

      // If no support and not passed, this is a backward pawn
      if (!hasSupport && !((pos.pieces(c, PAWN) & adjacentFiles) != 0)) {
        score -= 12;  // Backward pawn penalty
      }
    }

    // Pawn chain bonus: pawns defending each other diagonally
    Bitboard defenderMask = BB::pawnAttacks(~c, sq);
    if (defenderMask & pos.pieces(c, PAWN)) {
      score += 5;  // Small bonus for being part of a pawn chain
    }
  }

  return score;
}

int AI::evaluateKingSafety(const Position& pos, Color c) const {
  int score = 0;
  Square kingSq = BB::lsb(pos.pieces(c, KING));
  int kingFile = fileOf(kingSq);

  // Pawn shield bonus (pawns in front of king)
  Bitboard pawns = pos.pieces(c, PAWN);
  if (c == WHITE) {
    // Check pawns on rank 2 and 3 near king
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(1 * 8 + f)) score += 10;  // Pawn on 2nd rank
      if (pawns & BB::squareBB(2 * 8 + f)) score += 5;   // Pawn on 3rd rank
    }
  } else {
    // Check pawns on rank 7 and 6 near king
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(6 * 8 + f)) score += 10;  // Pawn on 7th rank
      if (pawns & BB::squareBB(5 * 8 + f)) score += 5;   // Pawn on 6th rank
    }
  }

  // Open file near king penalty
  for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
    Bitboard filePawns =
        (pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN)) & BB::fileBB(f);
    if (!filePawns) {
      score -= 20;  // Open file near king is dangerous
    }
  }

  return score;
}

int AI::evaluateMobility(const Position& pos, Color c) {
  // Count pseudo-legal moves as mobility metric
  int mobility = 0;

  // Knights
  Bitboard knights = pos.pieces(c, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    Bitboard attacks = BB::knightAttacks(sq) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Bishops
  Bitboard bishops = pos.pieces(c, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    Bitboard attacks =
        Magic::bishopAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Rooks
  Bitboard rooks = pos.pieces(c, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    Bitboard attacks = Magic::rookAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Queens
  Bitboard queens = pos.pieces(c, QUEEN);
  while (queens) {
    Square sq = BB::popLsb(queens);
    Bitboard attacks = (Magic::bishopAttacks(sq, pos.occupied()) |
                        Magic::rookAttacks(sq, pos.occupied())) &
                       ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Weight mobility (each move worth ~2 centipawns)
  return mobility * 2;
}

int AI::getGamePhase(const Position& pos) const {
  // Calculate game phase based on material
  // Opening: 256, Endgame: 0
  int phase = 0;

  // Knights and bishops: 1 point each
  phase += BB::popCount(pos.pieces(WHITE, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(WHITE, BISHOP)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, BISHOP)) * 1;

  // Rooks: 2 points each
  phase += BB::popCount(pos.pieces(WHITE, ROOK)) * 2;
  phase += BB::popCount(pos.pieces(BLACK, ROOK)) * 2;

  // Queens: 4 points each
  phase += BB::popCount(pos.pieces(WHITE, QUEEN)) * 4;
  phase += BB::popCount(pos.pieces(BLACK, QUEEN)) * 4;

  // Starting phase is 24 (4 knights + 4 bishops + 4 rooks + 2 queens = 4+4+8+8)
  // Scale to 256 for opening, 0 for endgame
  const int TOTAL_PHASE = 24;
  return std::min(256, (phase * 256 + TOTAL_PHASE / 2) / TOTAL_PHASE);
}

int AI::evaluateDevelopment(const Position& pos, Color c) const {
  int score = 0;

  // Penalty for pieces still on starting squares (only in opening/middlegame)
  if (c == WHITE) {
    // Knights on starting squares
    if (pos.pieceAt(B1) == makePiece(WHITE, KNIGHT)) score -= 20;
    if (pos.pieceAt(G1) == makePiece(WHITE, KNIGHT)) score -= 20;

    // Bishops on starting squares
    if (pos.pieceAt(C1) == makePiece(WHITE, BISHOP)) score -= 15;
    if (pos.pieceAt(F1) == makePiece(WHITE, BISHOP)) score -= 15;

    // Rooks on starting squares (less penalty)
    if (pos.pieceAt(A1) == makePiece(WHITE, ROOK)) score -= 5;
    if (pos.pieceAt(H1) == makePiece(WHITE, ROOK)) score -= 5;

    // Queen moved too early penalty
    Square queenSq = BB::lsb(pos.pieces(WHITE, QUEEN));
    if (queenSq != D1 && queenSq != NO_SQUARE) {
      // Queen is off starting square - check if minor pieces developed
      int minorsDeveloped = 0;
      if (pos.pieceAt(B1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;

      // Penalty if queen moved before developing pieces
      if (minorsDeveloped < 2) score -= 30;
    }

    // Castling bonus
    Square kingSq = BB::lsb(pos.pieces(WHITE, KING));
    if (kingSq == G1 || kingSq == C1) {
      score += 40;  // Big bonus for castling
    }

    // Center pawn bonus (e4/d4) - INCREASED to encourage central play
    if (pos.pieceAt(E4) == makePiece(WHITE, PAWN)) score += 50;
    if (pos.pieceAt(D4) == makePiece(WHITE, PAWN)) score += 50;

  } else {  // BLACK
    // Knights on starting squares
    if (pos.pieceAt(B8) == makePiece(BLACK, KNIGHT)) score -= 20;
    if (pos.pieceAt(G8) == makePiece(BLACK, KNIGHT)) score -= 20;

    // Bishops on starting squares
    if (pos.pieceAt(C8) == makePiece(BLACK, BISHOP)) score -= 15;
    if (pos.pieceAt(F8) == makePiece(BLACK, BISHOP)) score -= 15;

    // Rooks on starting squares (less penalty)
    if (pos.pieceAt(A8) == makePiece(BLACK, ROOK)) score -= 5;
    if (pos.pieceAt(H8) == makePiece(BLACK, ROOK)) score -= 5;

    // Queen moved too early penalty
    Square queenSq = BB::lsb(pos.pieces(BLACK, QUEEN));
    if (queenSq != D8 && queenSq != NO_SQUARE) {
      int minorsDeveloped = 0;
      if (pos.pieceAt(B8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;

      if (minorsDeveloped < 2) score -= 30;
    }

    // Castling bonus
    Square kingSq = BB::lsb(pos.pieces(BLACK, KING));
    if (kingSq == G8 || kingSq == C8) {
      score += 40;  // Big bonus for castling
    }

    // Center pawn bonus (e5/d5) - INCREASED to encourage central play
    if (pos.pieceAt(E5) == makePiece(BLACK, PAWN)) score += 50;
    if (pos.pieceAt(D5) == makePiece(BLACK, PAWN)) score += 50;
  }

  return score;
}

int AI::evaluateRooks(const Position& pos, Color c) const {
  int score = 0;
  Bitboard rooks = pos.pieces(c, ROOK);
  Bitboard ourPawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (rooks) {
    Square sq = BB::popLsb(rooks);
    int file = fileOf(sq);
    Bitboard fileMask = BB::fileBB(file);

    // Check if file is open (no pawns) or semi-open (no our pawns)
    bool hasOurPawns = (ourPawns & fileMask) != 0;
    bool hasEnemyPawns = (enemyPawns & fileMask) != 0;

    if (!hasOurPawns && !hasEnemyPawns) {
      // Rook on open file: +25 bonus
      score += 25;
    } else if (!hasOurPawns && hasEnemyPawns) {
      // Rook on semi-open file (attacking enemy pawns): +15 bonus
      score += 15;
    }

    // Bonus for rook on 7th rank (or 2nd rank for black)
    int rank = rankOf(sq);
    if ((c == WHITE && rank == RANK_7) || (c == BLACK && rank == RANK_2)) {
      score += 20;
    }
  }

  return score;
}

int AI::evaluateBishops(const Position& pos, Color c) const {
  int score = 0;
  Bitboard bishops = pos.pieces(c, BISHOP);

  // Bishop pair bonus: +30
  if (BB::popCount(bishops) >= 2) {
    score += 30;
  }

  return score;
}

int AI::evaluateKnights(const Position& pos, Color c) const {
  int score = 0;
  Bitboard knights = pos.pieces(c, KNIGHT);
  Bitboard ourPawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (knights) {
    Square sq = BB::popLsb(knights);
    int file = fileOf(sq);
    int rank = rankOf(sq);

    // Knight outpost detection:
    // - On 4th, 5th, or 6th rank (for white) / 5th, 4th, 3rd rank (for black)
    // - Defended by our pawn
    // - Cannot be attacked by enemy pawns

    bool isOutpostRank = false;
    if (c == WHITE && (rank == RANK_4 || rank == RANK_5 || rank == RANK_6)) {
      isOutpostRank = true;
    } else if (c == BLACK && (rank == RANK_5 || rank == RANK_4 || rank == RANK_3)) {
      isOutpostRank = true;
    }

    if (isOutpostRank) {
      // Check if defended by our pawn
      Bitboard defenderMask = BB::pawnAttacks(~c, sq);  // Squares from which our pawns would attack this square
      bool defendedByPawn = (defenderMask & ourPawns) != 0;

      if (defendedByPawn) {
        // Check if enemy pawns can attack this square
        bool canBeAttacked = false;

        // Check adjacent files for enemy pawns that could advance to attack
        if (c == WHITE) {
          // For white knight, check if black pawns on adjacent files ahead can attack
          for (int r = rank; r < 8; ++r) {
            if (file > 0 && (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 && (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        } else {
          // For black knight, check if white pawns on adjacent files ahead can attack
          for (int r = rank; r >= 0; --r) {
            if (file > 0 && (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 && (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        }

        if (!canBeAttacked) {
          // This is a true outpost! Bonus increases for central files
          int outpostBonus = 25;
          if (file >= 2 && file <= 5) {
            outpostBonus += 10;  // Extra bonus for central outpost
          }
          score += outpostBonus;
        }
      }
    }
  }

  return score;
}

int AI::getMoveScore(const Position& pos, Move move, int ply, Move ttMove) {
  // Hash move gets highest priority
  if (move == ttMove) {
    return 1000000;
  }

  int score = 0;
  Square from = fromSquare(move);
  Square to = toSquare(move);
  Piece captured = pos.pieceAt(to);

  // Prioritize captures using SEE
  if (captured != NO_PIECE || moveType(move) == EN_PASSANT) {
    // Use Static Exchange Evaluation for accurate capture ordering
    int seeValue = pos.see(move);

    if (seeValue > 0) {
      // Good capture - search early
      score += 20000 + seeValue;
    } else if (seeValue == 0) {
      // Equal trade - search after good captures but before quiet moves
      score += 10000;
    } else {
      // Bad capture - search after quiet moves
      score += 5000 + seeValue;  // seeValue is negative, so this lowers score
    }
  } else {
    // Quiet moves (non-captures)

    // Countermove heuristic (moves that refute previous move)
    if (ply > 0 && pvLength[ply - 1] > 0) {
      Move prevMove = pvTable[ply - 1][0];
      Move countermove = countermoves[fromSquare(prevMove)][toSquare(prevMove)];
      if (move == countermove) {
        score += 9500;  // Slightly higher than killer moves
      }
    }

    // Killer moves
    if (isKiller(move, ply)) {
      score += 9000;
    }

    // History heuristic
    score += historyTable[from][to];
  }

  // Prioritize promotions
  if (moveType(move) == PROMOTION) {
    score += 15000;
  }

  return score;
}

void AI::orderMoves(Position& pos, std::vector<Move>& moves, int ply,
                    Move ttMove) {
  std::sort(moves.begin(), moves.end(), [&](Move a, Move b) {
    return getMoveScore(pos, a, ply, ttMove) >
           getMoveScore(pos, b, ply, ttMove);
  });
}

void AI::updateHistory(Move move, int depth) {
  Square from = fromSquare(move);
  Square to = toSquare(move);

  // Bonus increases quadratically with depth
  int bonus = depth * depth;

  // Gravity-based aging: reduce score proportionally
  historyTable[from][to] +=
      bonus - historyTable[from][to] * std::abs(bonus) / 10000;

  // Clamp to prevent overflow
  if (historyTable[from][to] > 10000) {
    historyTable[from][to] = 10000;
  } else if (historyTable[from][to] < -10000) {
    historyTable[from][to] = -10000;
  }
}

void AI::storeKiller(Move move, int ply) {
  if (ply >= 64) return;

  // Shift killers and add new one
  if (killerMoves[ply][0] != move) {
    killerMoves[ply][1] = killerMoves[ply][0];
    killerMoves[ply][0] = move;
  }
}

bool AI::isKiller(Move move, int ply) const {
  if (ply >= 64) return false;
  return killerMoves[ply][0] == move || killerMoves[ply][1] == move;
}

// Syzygy tablebase methods
bool AI::initTablebases(const std::string& path) {
  return Tablebase::init(path);
}

void AI::freeTablebases() {
  Tablebase::free();
}

bool AI::hasTablebases() {
  return Tablebase::available();
}
