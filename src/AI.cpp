#include "AI.h"

#include "Bitboard.h"
#include "Eval.h"
#include "Logger.h"
#include "MoveGen.h"
#include "MovePicker.h"
#include "Tablebase.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>

// Strip halfmove clock and fullmove counter from a FEN string so that
// transpositions (same position, different move numbers) share a book key.
static std::string fenPositionKey(const std::string& fen) {
  // FEN fields: position side castling ep halfmove fullmove
  // We keep the first 4 fields.
  int spaces = 0;
  for (size_t i = 0; i < fen.size(); ++i) {
    if (fen[i] == ' ') {
      ++spaces;
      if (spaces == 4) return fen.substr(0, i);
    }
  }
  return fen;  // Fewer than 4 spaces — return as-is
}

// --- Static LMR table ---
int AI::lmrTable[64][64] = {};
bool AI::lmrInitialized = false;

void AI::initLMR() {
  for (int d = 0; d < 64; d++) {
    for (int m = 0; m < 64; m++) {
      if (d == 0 || m == 0) {
        lmrTable[d][m] = 0;
      } else {
        // Logarithmic formula: deeper depth and later moves get more reduction
        lmrTable[d][m] = static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25);
      }
    }
  }
  lmrInitialized = true;
}

AI::AI(int depth)
    : depth(depth),
      nodesSearched(0),
      ttHits(0),
      timeLimit(0),
      searchStartTime(0),
      stopSearch(false),
      moveCallback(nullptr),
      ttBucketCount((DEFAULT_TT_SIZE_MB * 1024 * 1024) / sizeof(TTBucket)),
      transpositionTable(ttBucketCount),
      ttAge(0),
      killerMoves{},
      historyTable{},
      pvTable{},
      pvLength{},
      countermoves{} {
  if (!lmrInitialized) initLMR();
  std::ostringstream oss;
  oss << "AI::AI() constructor called (this=" << this << ")";
  Logger::getInstance().debug(oss.str());
}

// Helper to get current time in milliseconds
static uint64_t currentTimeMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
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
    if (line.empty() || line[0] == '#') continue;

    size_t pipePos = line.find('|');
    if (pipePos == std::string::npos) continue;

    std::string fen = line.substr(0, pipePos);
    std::string movesStr = line.substr(pipePos + 1);

    fen.erase(0, fen.find_first_not_of(" \t"));
    fen.erase(fen.find_last_not_of(" \t") + 1);

    std::istringstream iss(movesStr);
    std::string moveStr;
    std::vector<Move> moves;

    Position bookPos;
    if (!bookPos.setFromFEN(fen)) {
      std::cerr << "Warning: Invalid FEN in book line " << lineCount << ": " << fen << std::endl;
      continue;
    }

    while (iss >> moveStr) {
      if (moveStr.length() < 4) continue;

      Square from = Square((moveStr[1] - '1') * 8 + (moveStr[0] - 'a'));
      Square to = Square((moveStr[3] - '1') * 8 + (moveStr[2] - 'a'));

      PieceType promotion = NO_PIECE_TYPE;
      if (moveStr.length() == 5) {
        char promChar = moveStr[4];
        if (promChar == 'q')
          promotion = QUEEN;
        else if (promChar == 'r')
          promotion = ROOK;
        else if (promChar == 'b')
          promotion = BISHOP;
        else if (promChar == 'n')
          promotion = KNIGHT;
      }

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
      openingBook[fenPositionKey(fen)] = moves;
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

  std::string fen = fenPositionKey(pos.getFEN());
  auto it = openingBook.find(fen);
  if (it == openingBook.end()) return 0;

  const std::vector<Move>& bookMoves = it->second;
  if (bookMoves.empty()) return 0;

  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<> dis(0, static_cast<int>(bookMoves.size()) - 1);

  return bookMoves[dis(gen)];
}

bool AI::hasOpeningBook() const {
  return !openingBook.empty();
}

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
  // Check opening books if enabled and within move limit
  bool inBookRange = useOwnBook && (bookMoveLimit == 0 || pos.fullmoveNumber() <= bookMoveLimit);

  if (inBookRange) {
    // Check Polyglot book first
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
  }

  // Check Syzygy tablebases at root
  if (Tablebase::available() && Tablebase::canProbe(pos)) {
    TBProbeResult tbResult = Tablebase::probeRoot(pos);
    if (tbResult.success && tbResult.bestMove != 0) {
      const char* wdlStr = "unknown";
      switch (tbResult.wdl) {
        case TB_RESULT_WIN:
          wdlStr = "win";
          break;
        case TB_RESULT_CURSED_WIN:
          wdlStr = "cursed win";
          break;
        case TB_RESULT_DRAW:
          wdlStr = "draw";
          break;
        case TB_RESULT_BLESSED_LOSS:
          wdlStr = "blessed loss";
          break;
        case TB_RESULT_LOSS:
          wdlStr = "loss";
          break;
        default:
          break;
      }
      std::cout << "info string Tablebase hit: " << wdlStr << " (DTZ: " << tbResult.dtz << ")"
                << std::endl;
      return tbResult.bestMove;
    }
  }

  std::cout << "info string Searching (no book move)..." << std::endl;
  nodesSearched = 0;
  ttHits = 0;
  ttAge = (ttAge + 1) & 63;  // Increment age (wraps at 64)

  std::vector<Move> rootMoves = MoveGen::generateLegalMoves(pos);
  if (rootMoves.empty()) return 0;

  Move bestMove = rootMoves[0];
  int bestScore = std::numeric_limits<int>::min();

  std::cout << "Using iterative deepening:\n";

  // Iterative deepening
  for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
    if (shouldStop()) {
      std::cout << "  Time limit reached, stopping at depth " << (currentDepth - 1) << "\n";
      break;
    }

    // Aspiration windows
    int alpha, beta;
    if (currentDepth >= 5 && bestScore != std::numeric_limits<int>::min()) {
      const int ASPIRATION_WINDOW = 50;
      alpha = bestScore - ASPIRATION_WINDOW;
      beta = bestScore + ASPIRATION_WINDOW;
    } else {
      alpha = std::numeric_limits<int>::min() + 1;
      beta = std::numeric_limits<int>::max() - 1;
    }

    // Order moves based on previous iteration
    auto scoredRootMoves = orderMoves(pos, rootMoves, 0, bestMove);
    rootMoves.clear();
    for (const auto& sm : scoredRootMoves) {
      rootMoves.push_back(sm.move);
    }

    Move iterBestMove = rootMoves[0];
    int iterBestScore = std::numeric_limits<int>::min();

    std::cout << "  Depth " << currentDepth << ": ";
    std::cout.flush();

    for (Move move : rootMoves) {
      std::cout << moveToString(move) << " ";
      std::cout.flush();

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

    // Re-search with wider window on aspiration failure
    if (currentDepth >= 5 && (iterBestScore <= bestScore - 50 || iterBestScore >= bestScore + 50)) {
      std::cout << " [re-search " << moveToString(iterBestMove) << "]";
      alpha = std::numeric_limits<int>::min() + 1;
      beta = std::numeric_limits<int>::max() - 1;

      pos.makeMove(iterBestMove);
      iterBestScore = -negamax(pos, currentDepth - 1, -beta, -alpha, 1);
      pos.unmakeMove();
    }

    bestMove = iterBestMove;
    bestScore = iterBestScore;

    std::cout << "\n  Depth " << currentDepth << " complete: " << moveToString(bestMove)
              << " (score: " << bestScore << ", nodes: " << nodesSearched << ", tt hits: " << ttHits
              << ")\n";
  }

  std::cout << "Best move: " << moveToString(bestMove) << " (score: " << bestScore << ")\n";
  std::cout << "Total nodes: " << nodesSearched << ", TT hits: " << ttHits << "\n";

  return bestMove;
}

// =============================================================================
// negamax — the core search function with all 10 improvements
// =============================================================================
int AI::negamax(Position& pos, int depth, int alpha, int beta, int ply, Move excludedMove) {
  nodesSearched++;

  // Prefetch TT bucket early — cache line loads while we do draw detection
  prefetchTT(pos.hash());

  // Periodic time check (every 1024 nodes)
  if ((nodesSearched & 0x3FF) == 0 && shouldStop()) {
    return 0;
  }

  // Draw detection (skip root)
  if (ply > 0) {
    if (pos.repetitionCount() >= 2 || pos.halfmoveClock() >= 100) {
      // Phase-scaled contempt: fight harder in endgames (where passive
      // draws cost Elo) but stay neutral in middlegames.
      int phase = pos.getGamePhase();
      int contempt = -CONTEMPT_MAX * (256 - phase) / 256;
      int material = pos.materialCount(pos.sideToMove()) - pos.materialCount(~pos.sideToMove());
      if (material > CONTEMPT_MATERIAL_THRESHOLD)
        contempt -= CONTEMPT_AHEAD_BONUS;
      else if (material < -CONTEMPT_MATERIAL_THRESHOLD)
        contempt += CONTEMPT_BEHIND_OFFSET;
      return contempt;
    }
  }

  // Syzygy WDL probe (depth >= 2, not root)
  if (ply > 0 && depth >= 2 && Tablebase::available() && Tablebase::canProbe(pos) &&
      pos.castlingRights() == 0) {
    TBResult wdl = Tablebase::probeWDL(pos);
    if (wdl != TB_RESULT_UNKNOWN) {
      int tbScore = Tablebase::wdlToScore(wdl, ply);
      storeTT(pos.hash(), depth, tbScore, 0, alpha, beta, ply);
      return tbScore;
    }
  }

  int alphaOrig = alpha;
  HashKey hash = pos.hash();
  TTProbeInfo ttInfo;

  // [Improvement 3+6] Multi-bucket TT probe with mate score adjustment
  if (excludedMove == 0) {
    if (auto score = probeTT(hash, depth, alpha, beta, ply, ttInfo)) {
      return *score;
    }
  } else {
    // During singular search, only get TT move, no cutoffs.
    // Use local copies of alpha/beta so the TT probe doesn't
    // corrupt the outer search window.
    int tmpAlpha = std::numeric_limits<int>::min() + 1;
    int tmpBeta = std::numeric_limits<int>::max() - 1;
    probeTT(hash, 0, tmpAlpha, tmpBeta, ply, ttInfo);
  }

  Move ttMove = ttInfo.ttMove;

  // Quiescence search at leaf nodes
  if (depth <= 0) {
    pvLength[ply] = 0;
    return quiescence(pos, alpha, beta, 0);
  }

  // [Improvement 9] Adaptive null move pruning
  if (auto score = tryNullMovePruning(pos, depth, alpha, beta, ply)) {
    return *score;
  }

  bool isPVNode = (beta - alpha) > 1;

  // [Improvement 10] Fix: futility pruning NOT applied at PV nodes
  auto pruning = canPrune(pos, depth, alpha, beta, isPVNode);
  if (pruning.cutoff) return pruning.score;
  bool futilityPrune = pruning.futilityPrune;

  // [Improvement 4] Singular Extension
  int singularExtension = 0;
  if (depth >= 8 && ttMove != 0 && excludedMove == 0 && !isPVNode && ttInfo.found &&
      ttInfo.ttDepth >= depth - 3 && ttInfo.ttFlag != UPPERBOUND &&
      std::abs(ttInfo.ttScore) < MATE_BOUND) {
    int singularBeta = ttInfo.ttScore - 2 * depth;
    int singularDepth = (depth - 1) / 2;
    int singularScore = negamax(pos, singularDepth, singularBeta - 1, singularBeta, ply, ttMove);
    if (singularScore < singularBeta) {
      singularExtension = 1;
    }
  }

  // IID: if no hash move at PV node, do shallow search to find one
  if (!ttMove && isPVNode && depth >= 4) {
    int iidDepth = depth - 2;
    (void)negamax(pos, iidDepth, alpha, beta, ply);
    // Re-probe TT to get the move found by IID
    TTProbeInfo iidInfo;
    int tmpAlpha = alpha, tmpBeta = beta;
    (void)probeTT(hash, iidDepth, tmpAlpha, tmpBeta, ply, iidInfo);
    if (iidInfo.ttMove != 0) {
      ttMove = iidInfo.ttMove;
    }
  }

  // [Improvement 1] Staged move generation via MovePicker
  Move k1 = (ply < 64) ? killerMoves[ply][0] : Move(0);
  Move k2 = (ply < 64) ? killerMoves[ply][1] : Move(0);

  // Compute countermove
  Move cm = 0;
  const auto& hist = pos.getHistory();
  if (!hist.empty()) {
    Move prevMove = hist.back().move;
    cm = countermoves[fromSquare(prevMove)][toSquare(prevMove)];
  }

  MovePicker picker(pos, ttMove, k1, k2, cm, historyTable, excludedMove);

  int maxScore = std::numeric_limits<int>::min();
  Move bestMove = 0;
  pvLength[ply] = 0;
  int moveNum = 0;

  // Track quiet moves tried (for history malus on cutoff)
  Move quietsTried[64];
  int numQuietsTried = 0;

  Move move;
  while ((move = picker.next()) != 0) {
    Square to = toSquare(move);
    bool isCapture = pos.pieceAt(to) != NO_PIECE || moveType(move) == EN_PASSANT;
    bool isPromotion = moveType(move) == PROMOTION;
    bool isKillerMove = isKiller(move, ply);

    // Futility pruning: skip quiet moves if position is hopeless
    // [Improvement 10] Only at non-PV nodes (enforced by canPrune)
    if (futilityPrune && moveNum > 0 && !isCapture && !isPromotion) {
      continue;
    }

    // Late Move Pruning: skip late quiet moves at low depths
    if (depth <= 3 && moveNum >= static_cast<int>(3 + depth * depth) && !isCapture &&
        !isPromotion && !isKillerMove) {
      continue;
    }

    // Lazy legality: make the move, check if our king is in check
    pos.makeMove(move);
    Color us = ~pos.sideToMove();  // Side that just moved
    if (pos.isAttacked(BB::lsb(pos.pieces(us, KING)), pos.sideToMove())) {
      pos.unmakeMove();
      continue;  // Illegal move
    }

    bool givesCheck = pos.inCheck();
    int extension = givesCheck ? 1 : 0;

    // Apply singular extension for the TT move
    if (move == ttMove) extension = std::max(extension, singularExtension);

    int newDepth = depth - 1 + extension;

    // [Improvement 2] Better LMR with logarithmic table
    int score;
    if (moveNum == 0) {
      // First move: full window search
      score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
    } else {
      int reduction = 0;

      if (depth >= 3 && moveNum >= 3 && !isCapture && !givesCheck && !isPromotion) {
        // Base reduction from precomputed table
        reduction = lmrTable[std::min(depth, 63)][std::min(moveNum, 63)];

        // Adjustments:
        if (isPVNode) reduction--;      // Reduce less at PV
        if (isKillerMove) reduction--;  // Reduce less for killers
        if (move == cm) reduction--;    // Reduce less for countermove

        // [Improvement 7] History-based adjustment
        int histScore = historyTable[fromSquare(move)][toSquare(move)];
        reduction -= histScore / 5000;  // Good history reduces less

        // Clamp: at least 0, at most newDepth-1
        reduction = std::max(0, std::min(reduction, newDepth - 1));
      }

      // PVS: null-window search (possibly with reduction)
      score = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1);

      // Re-search at full depth if reduced search beat alpha
      if (score > alpha && reduction > 0) {
        score = -negamax(pos, newDepth, -alpha - 1, -alpha, ply + 1);
      }

      // Full PV search if still inside window
      if (score > alpha && score < beta) {
        score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
      }
    }

    pos.unmakeMove();

    // Track tried quiet moves for history malus
    if (!isCapture && !isPromotion && numQuietsTried < 64) {
      quietsTried[numQuietsTried++] = move;
    }

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
      // Beta cutoff
      if (!isCapture) {
        storeKiller(move, ply);
        updateHistory(move, depth * depth);  // Bonus for cutoff move

        // [Improvement 7] History malus: penalize all quiet moves tried
        // before the cutoff move
        for (int i = 0; i < numQuietsTried - 1; i++) {
          updateHistory(quietsTried[i], -(depth * depth));
        }

        // Countermove heuristic
        if (!hist.empty()) {
          Move prevMove = hist.back().move;
          countermoves[fromSquare(prevMove)][toSquare(prevMove)] = move;
        }
      }
      break;
    }

    moveNum++;
  }

  // Checkmate or stalemate detection
  if (moveNum == 0 && maxScore == std::numeric_limits<int>::min()) {
    if (excludedMove != 0) {
      // In singular search, we excluded a move — not truly no legal moves
      return alpha;
    }
    if (pos.inCheck()) {
      return -MATE_SCORE + ply;  // Checkmate
    } else {
      return 0;  // Stalemate
    }
  }

  // [Improvement 3+6] Store to multi-bucket TT with mate score adjustment
  if (excludedMove == 0) {
    storeTT(hash, depth, maxScore, bestMove, alphaOrig, beta, ply);
  }

  return maxScore;
}

// =============================================================================
// Quiescence search (unchanged except minor cleanups)
// =============================================================================
int AI::quiescence(Position& pos, int alpha, int beta, int qsDepth) {
  nodesSearched++;

  if (pos.repetitionCount() >= 2) {
    int phase = pos.getGamePhase();
    int contempt = -CONTEMPT_MAX * (256 - phase) / 256;
    int material = pos.materialCount(pos.sideToMove()) - pos.materialCount(~pos.sideToMove());
    if (material > CONTEMPT_MATERIAL_THRESHOLD)
      contempt -= CONTEMPT_AHEAD_BONUS;
    else if (material < -CONTEMPT_MATERIAL_THRESHOLD)
      contempt += CONTEMPT_BEHIND_OFFSET;
    return contempt;
  }

  bool inCheck = pos.inCheck();
  int standPat = 0;

  if (!inCheck) {
    standPat = Eval::evaluate(pos);
    if (standPat >= beta) return beta;

    constexpr int DELTA_MARGIN = 900;
    if (standPat + DELTA_MARGIN < alpha) return alpha;
    if (alpha < standPat) alpha = standPat;
  }

  std::vector<Move> captures;
  if (inCheck) {
    captures = MoveGen::generateLegalMoves(pos);
    if (captures.empty()) {
      return -MATE_SCORE + static_cast<int>(pos.getHistory().size());
    }
  } else {
    captures = MoveGen::generateCaptures(pos);
    if (qsDepth == 0) {
      std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
      captures.insert(captures.end(), checks.begin(), checks.end());
    }
  }

  std::vector<ScoredMove> scoredCaptures;
  scoredCaptures.reserve(captures.size());
  for (Move m : captures) {
    scoredCaptures.push_back(scoreMoveWithSEE(pos, m, 0, 0));
  }
  std::sort(scoredCaptures.begin(), scoredCaptures.end(),
            [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

  for (const ScoredMove& sm : scoredCaptures) {
    if (sm.seeValue != std::numeric_limits<int>::min() && sm.seeValue < 0) {
      continue;
    }

    Square to = toSquare(sm.move);
    Piece captured = pos.pieceAt(to);
    if (captured != NO_PIECE) {
      static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
      if (standPat + pieceValues[typeOf(captured)] + 200 < alpha) {
        continue;
      }
    }

    pos.makeMove(sm.move);

    // Lazy legality: skip illegal pseudo-legal captures (e.g., pinned pieces)
    Color qUs = ~pos.sideToMove();
    if (pos.isAttacked(BB::lsb(pos.pieces(qUs, KING)), pos.sideToMove())) {
      pos.unmakeMove();
      continue;
    }

    int score = -quiescence(pos, -beta, -alpha, qsDepth + 1);
    pos.unmakeMove();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }

  return alpha;
}

// =============================================================================
// Move scoring (used for root move ordering and qsearch)
// =============================================================================
ScoredMove AI::scoreMoveWithSEE(const Position& pos, Move move, int ply, Move ttMove) {
  ScoredMove sm;
  sm.move = move;
  sm.seeValue = std::numeric_limits<int>::min();

  if (move == ttMove) {
    sm.score = 1000000;
    return sm;
  }

  sm.score = 0;
  Square from = fromSquare(move);
  Square to = toSquare(move);
  Piece captured = pos.pieceAt(to);

  if (captured != NO_PIECE || moveType(move) == EN_PASSANT) {
    sm.seeValue = pos.see(move);
    if (sm.seeValue > 0) {
      sm.score = 20000 + sm.seeValue;
    } else if (sm.seeValue == 0) {
      sm.score = 10000;
    } else {
      sm.score = 5000 + sm.seeValue;
    }
  } else {
    // Quiet moves
    if (ply > 0) {
      const auto& hist = pos.getHistory();
      if (!hist.empty()) {
        Move prevMove = hist.back().move;
        Move cm = countermoves[fromSquare(prevMove)][toSquare(prevMove)];
        if (move == cm) {
          sm.score = 9500;
        }
      }
    }
    if (isKiller(move, ply)) {
      sm.score += 9000;
    }
    sm.score += historyTable[from][to];

    // Retreat penalty
    const auto& hist = pos.getHistory();
    if (hist.size() >= 2) {
      const auto& prevState = hist[hist.size() - 2];
      if (from == toSquare(prevState.move) && to == fromSquare(prevState.move)) {
        sm.score -= 3000;
      }
    }
  }

  if (moveType(move) == PROMOTION) {
    sm.score += 15000;
  }

  return sm;
}

std::vector<ScoredMove> AI::orderMoves(Position& pos, const std::vector<Move>& moves, int ply,
                                       Move ttMove) {
  std::vector<ScoredMove> scored;
  scored.reserve(moves.size());
  for (Move m : moves) {
    scored.push_back(scoreMoveWithSEE(pos, m, ply, ttMove));
  }
  std::sort(scored.begin(), scored.end(),
            [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });
  return scored;
}

// =============================================================================
// [Improvement 7] History heuristic with malus support
// =============================================================================
void AI::updateHistory(Move move, int bonus) {
  Square from = fromSquare(move);
  Square to = toSquare(move);

  // Gravity-based aging: prevents scores from growing unbounded
  historyTable[from][to] += bonus - historyTable[from][to] * std::abs(bonus) / 10000;

  // Clamp
  if (historyTable[from][to] > 10000) {
    historyTable[from][to] = 10000;
  } else if (historyTable[from][to] < -10000) {
    historyTable[from][to] = -10000;
  }
}

void AI::storeKiller(Move move, int ply) {
  if (ply >= 64) return;
  if (killerMoves[ply][0] != move) {
    killerMoves[ply][1] = killerMoves[ply][0];
    killerMoves[ply][0] = move;
  }
}

bool AI::isKiller(Move move, int ply) const {
  if (ply >= 64) return false;
  return killerMoves[ply][0] == move || killerMoves[ply][1] == move;
}

// =============================================================================
// [Improvement 9] Adaptive null move pruning
// =============================================================================
std::optional<int> AI::tryNullMovePruning(Position& pos, int depth, int /*alpha*/, int beta,
                                          int ply) {
  bool canDoNullMove = depth >= 3 && !pos.inCheck() && ply > 0;

  if (canDoNullMove) {
    Color us = pos.sideToMove();
    int material = pos.materialCount(us);

    int knights = BB::popCount(pos.pieces(us, KNIGHT));
    int bishops = BB::popCount(pos.pieces(us, BISHOP));
    int rooks = BB::popCount(pos.pieces(us, ROOK));
    int queens = BB::popCount(pos.pieces(us, QUEEN));
    int nonPawnPieces = knights + bishops + rooks + queens;

    if (material <= 100 || nonPawnPieces == 0 || (nonPawnPieces == 1 && material < 500)) {
      canDoNullMove = false;
    }
  }

  if (canDoNullMove) {
    // Adaptive R: higher reduction at greater depths
    int R = 3 + depth / 6;

    // Eval-based boost: if static eval is well above beta, increase R
    int eval = Eval::evaluate(pos);
    if (eval > beta + 200) R++;

    R = std::min(R, depth - 1);  // Don't reduce below depth 1

    pos.makeNullMove();
    int nullDepth = std::max(0, depth - 1 - R);
    int score = -negamax(pos, nullDepth, -beta, -beta + 1, ply + 1);
    pos.unmakeNullMove();

    if (score >= beta) {
      return beta;
    }
  }

  return std::nullopt;
}

// =============================================================================
// [Improvement 10] Fix: futility pruning NOT at PV nodes
// =============================================================================
AI::PruningResult AI::canPrune(Position& pos, int depth, int alpha, int beta, bool isPVNode) {
  PruningResult result = {false, 0, false};

  if (pos.inCheck()) return result;

  int eval = Eval::evaluate(pos);

  // Reverse Futility Pruning — non-PV nodes only
  if (depth <= 6 && !isPVNode) {
    int rfpMargin = 100 * depth;
    if (eval - rfpMargin >= beta) {
      result.cutoff = true;
      result.score = eval;
      return result;
    }
  }

  // Razoring — non-PV nodes only
  if (depth <= 3 && !isPVNode) {
    int razoringMargin = 300 + 150 * depth;
    if (eval + razoringMargin < alpha) {
      int qscore = quiescence(pos, alpha, beta, 0);
      if (qscore < alpha) {
        result.cutoff = true;
        result.score = qscore;
        return result;
      }
    }
  }

  // Futility flag — NOT at PV nodes (this was the bug)
  if (depth <= 3 && !isPVNode) {
    int futilityMargin = 100 + 200 * depth;
    int futilityValue = eval + futilityMargin;
    if (futilityValue <= alpha) {
      result.futilityPrune = true;
    }
  }

  return result;
}

// =============================================================================
// [Improvement 3+6] Multi-bucket TT with mate score adjustment
// =============================================================================
std::optional<int> AI::probeTT(HashKey hash, int depth, int& alpha, int& beta, int ply,
                               TTProbeInfo& info) {
  size_t bucketIdx = hash % ttBucketCount;
  uint32_t key32 = static_cast<uint32_t>(hash >> 32);
  TTBucket& bucket = transpositionTable[bucketIdx];

  for (int i = 0; i < TT_BUCKET_SIZE; i++) {
    TTEntry& entry = bucket.entries[i];
    if (entry.key32 == key32) {
      info.ttMove = entry.bestMove;
      info.ttDepth = entry.depth;
      info.ttFlag = entry.getFlag();
      info.found = true;

      // Mate score adjustment: convert from position-relative to ply-relative
      int score = entry.score;
      if (score > MATE_BOUND)
        score -= ply;
      else if (score < -MATE_BOUND)
        score += ply;
      info.ttScore = static_cast<int16_t>(score);

      if (entry.depth >= depth) {
        ttHits++;
        if (entry.getFlag() == EXACT) {
          return score;
        } else if (entry.getFlag() == LOWERBOUND) {
          alpha = std::max(alpha, score);
        } else if (entry.getFlag() == UPPERBOUND) {
          beta = std::min(beta, score);
        }

        if (alpha >= beta) {
          return score;
        }
      }
      return std::nullopt;
    }
  }

  info = TTProbeInfo{};  // Not found
  return std::nullopt;
}

void AI::storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta,
                 int ply) {
  size_t bucketIdx = hash % ttBucketCount;
  uint32_t key32 = static_cast<uint32_t>(hash >> 32);
  TTBucket& bucket = transpositionTable[bucketIdx];

  // Mate score adjustment: convert from ply-relative to position-relative.
  // A winning mate score (e.g., MATE_SCORE - ply) must be stored as the
  // absolute distance, so subtract ply to get closer to MATE_SCORE.
  // A losing mate score (e.g., -MATE_SCORE + ply) must add ply (more negative).
  int adjScore = score;
  if (adjScore > MATE_BOUND)
    adjScore -= ply;
  else if (adjScore < -MATE_BOUND)
    adjScore += ply;

  // Determine flag
  TTFlag flag;
  if (score <= alphaOrig) {
    flag = UPPERBOUND;
  } else if (score >= beta) {
    flag = LOWERBOUND;
  } else {
    flag = EXACT;
  }

  // Find the best slot to replace in the bucket
  int replaceIdx = 0;
  int worstPriority = std::numeric_limits<int>::max();

  for (int i = 0; i < TT_BUCKET_SIZE; i++) {
    TTEntry& entry = bucket.entries[i];

    // Empty slot — use immediately
    if (entry.isEmpty()) {
      replaceIdx = i;
      break;
    }

    // Same position — always replace
    if (entry.key32 == key32) {
      replaceIdx = i;
      break;
    }

    // Compute replacement priority: prefer replacing shallow + old entries
    int ageDiff = (ttAge - entry.getAge()) & 63;
    int priority = static_cast<int>(entry.depth) * 4 - ageDiff * 8;
    if (priority < worstPriority) {
      worstPriority = priority;
      replaceIdx = i;
    }
  }

  TTEntry& target = bucket.entries[replaceIdx];
  target.key32 = key32;
  target.score = static_cast<int16_t>(std::max(std::min(adjScore, 32000), -32000));
  target.bestMove = bestMove;
  target.depth = static_cast<int8_t>(std::min(depth, 127));
  target.setFlagAge(flag, ttAge);
}

// =============================================================================
// Syzygy tablebase methods
// =============================================================================
bool AI::initTablebases(const std::string& path) {
  return Tablebase::init(path);
}

void AI::freeTablebases() {
  Tablebase::free();
}

bool AI::hasTablebases() {
  return Tablebase::available();
}
