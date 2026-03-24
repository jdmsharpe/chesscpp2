#include "AI.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <fstream>
#include <sstream>
#include <random>

#include "Bitboard.h"
#include "Eval.h"
#include "Logger.h"
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

// Polyglot book methods
bool AI::loadPolyglotBook(const std::string& filename) {
  return polyglotBook.load(filename);
}

Move AI::probePolyglotBook(const Position& pos) {
  return polyglotBook.probe(pos);
}

bool AI::hasPolyglotBook() const {
  return polyglotBook.isLoaded();
}

Move AI::findBestMove(Position& pos, int timeMs) {
  timeLimit = timeMs;
  searchStartTime = currentTimeMs();
  stopSearch = false;
  return findBestMove(pos);
}

Move AI::findBestMove(Position& pos) {
  // Check Polyglot book first (if loaded)
  if (hasPolyglotBook()) {
    Move bookMove = probePolyglotBook(pos);
    if (bookMove != 0) {
      std::cout << "info string Polyglot book move: " << moveToString(bookMove) << std::endl;
      return bookMove;
    }
  }

  // Fall back to text opening book
  Move bookMove = probeOpeningBook(pos);
  if (bookMove != 0) {
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
  Move ttMove = 0;

  if (auto score = probeTT(hash, depth, alpha, beta, ttMove)) {
    return *score;
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
    int eval = Eval::evaluate(pos);
    int rfpMargin = 100 * depth;  // 100, 200, 300... 600 for depths 1-6
    if (eval - rfpMargin >= beta) {
      return eval;  // Position too good, opponent won't allow this line
    }
  }

  // Razoring
  // If position is so bad that even qsearch can't save it, return qsearch score
  if (depth <= 3 && !isPVNode && !pos.inCheck()) {
    int eval = Eval::evaluate(pos);
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
    int futilityValue = Eval::evaluate(pos) + futilityMargin;

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

  // Internal Iterative Deepening (IID)
  // If no hash move at PV node, do shallow search to find one
  if (!ttMove && isPVNode && depth >= 4) {
    int iidDepth = depth - 2;
    (void)negamax(pos, iidDepth, alpha, beta, ply);
    // Re-probe TT to get the move found by IID
    Move iidMove = 0;
    int tmpAlpha = alpha, tmpBeta = beta;
    (void)probeTT(hash, depth, tmpAlpha, tmpBeta, iidMove);
    if (iidMove != 0) {
      bool found = false;
      for (Move m : moves) {
        if (m == iidMove) { found = true; break; }
      }
      if (found) ttMove = iidMove;
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

  storeTT(hash, depth, maxScore, bestMove, alphaOrig, beta);

  return maxScore;
}

int AI::quiescence(Position& pos, int alpha, int beta, int qsDepth) {
  nodesSearched++;

  // Check extension: if in check, search deeper
  bool inCheck = pos.inCheck();

  // Stand pat score (not valid if in check - must move)
  int standPat = 0;
  if (!inCheck) {
    standPat = Eval::evaluate(pos);

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

std::optional<int> AI::probeTT(HashKey hash, int depth, int& alpha, int& beta, Move& ttMove) {
  size_t ttIndex = hash % TT_SIZE;
  TTEntry& ttEntry = transpositionTable[ttIndex];

  if (ttEntry.key == hash) {
    ttMove = ttEntry.bestMove;

    if (ttEntry.depth >= depth) {
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
  }

  return std::nullopt;
}

void AI::storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta) {
  size_t ttIndex = hash % TT_SIZE;
  TTEntry& entry = transpositionTable[ttIndex];

  bool shouldReplace = (entry.key == 0) ||
                       (entry.key == hash) ||
                       (entry.depth <= depth) ||
                       (entry.age != ttAge);

  if (shouldReplace) {
    entry.key = hash;
    entry.depth = depth;
    entry.score = score;
    entry.bestMove = bestMove;
    entry.age = ttAge;

    if (score <= alphaOrig) {
      entry.flag = UPPERBOUND;
    } else if (score >= beta) {
      entry.flag = LOWERBOUND;
    } else {
      entry.flag = EXACT;
    }
  }
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
