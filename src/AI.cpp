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
#include <thread>

namespace {

constexpr int ROOT_FULL_ALPHA = std::numeric_limits<int>::min() + 1;
constexpr int ROOT_FULL_BETA = std::numeric_limits<int>::max() - 1;
constexpr int MAIN_ASPIRATION_WINDOW = 50;
constexpr int HELPER_WIDE_ASPIRATION_WINDOW = 75;
constexpr int HELPER_NARROW_ASPIRATION_WINDOW = 35;

}  // namespace

// Strip halfmove clock and fullmove counter from a FEN string so that
// transpositions (same position, different move numbers) share a book key.
static std::string fenPositionKey(const std::string& fen) {
  int spaces = 0;
  for (size_t i = 0; i < fen.size(); ++i) {
    if (fen[i] == ' ') {
      ++spaces;
      if (spaces == 4) return fen.substr(0, i);
    }
  }
  return fen;
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
        lmrTable[d][m] = static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25);
      }
    }
  }
  lmrInitialized = true;
}

AI::AI(int depth)
    : depth(depth),
      timeLimit(0),
      searchStartTime(0),
      stopSearch(false),
      moveCallback(nullptr),
      ttBucketCount((DEFAULT_TT_SIZE_MB * 1024 * 1024) / sizeof(TTBucket)),
      transpositionTable(ttBucketCount),
      ttAge(0) {
  if (!lmrInitialized) initLMR();
  Logger::getInstance().debug("AI::AI() constructor called");
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

uint64_t AI::getNodesSearched() const {
  uint64_t total = 0;
  for (const auto& td : threads) total += td.nodesSearched;
  return total;
}

uint64_t AI::getTTHits() const {
  uint64_t total = 0;
  for (const auto& td : threads) total += td.ttHits;
  return total;
}

// =============================================================================
// Opening book implementation
// =============================================================================
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

      MoveList legalMoves = MoveGen::generateLegalMoves(bookPos);
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
  oss << "Loaded opening book with " << openingBook.size() << " positions";
  Logger::getInstance().info(oss.str());
}

Move AI::probeOpeningBook(const Position& pos) {
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

// =============================================================================
// findBestMove — Lazy SMP parallel search
// =============================================================================
Move AI::findBestMove(Position& pos, int timeMs) {
  timeLimit = timeMs;
  return findBestMove(pos);
}

Move AI::findBestMove(Position& pos) {
  searchStartTime = currentTimeMs();
  stopSearch.store(false);
  lastSelectedThreadId = 0;
  lastSelectedDepth = 0;

  // Check opening books if enabled and within move limit
  bool inBookRange = useOwnBook && (bookMoveLimit == 0 || pos.fullmoveNumber() <= bookMoveLimit);

  if (inBookRange) {
    if (hasPolyglotBook()) {
      Move bookMove = probePolyglotBook(pos);
      if (bookMove != 0) {
        std::cout << "info string Polyglot book move: " << moveToString(bookMove) << std::endl;
        return bookMove;
      }
    }
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
  ttAge = (ttAge + 1) & 63;

  MoveList rootMoves = MoveGen::generateLegalMoves(pos);
  if (rootMoves.empty()) return 0;

  // Initialize thread data
  threads.resize(numThreads);
  for (int t = 0; t < numThreads; ++t) {
    threads[t].threadId = t;
    threads[t].pos = pos;
    threads[t].clear();
    threads[t].bestMove = rootMoves[0];
  }

  // Launch helper threads (Lazy SMP: independent searches sharing TT)
  std::vector<std::thread> helpers;
  for (int t = 1; t < numThreads; ++t) {
    helpers.emplace_back(&AI::helperThreadSearch, this, std::ref(threads[t]), rootMoves, depth);
  }

  // Main thread (thread 0) does the iterative deepening with reporting
  ThreadData& td = threads[0];

  std::cout << "Using iterative deepening";
  if (numThreads > 1) std::cout << " (" << numThreads << " threads)";
  std::cout << ":\n";

  for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
    if (shouldStop()) {
      std::cout << "  Time limit reached, stopping at depth " << td.completedDepth << "\n";
      break;
    }

    // Order moves based on previous iteration
    Move previousBestMove = (td.completedDepth > 0) ? td.bestMove : Move(0);
    ScoredMoveList scoredRootMoves = orderMoves(td, rootMoves, 0, previousBestMove);
    rootMoves.clear();
    for (size_t ri = 0; ri < scoredRootMoves.size(); ++ri) {
      rootMoves.push_back(scoredRootMoves[ri].move);
    }

    RootWindow window = getRootAspirationWindow(td, currentDepth);

    std::cout << "  Depth " << currentDepth << ": ";
    std::cout.flush();

    RootSearchResult result =
        searchRoot(td, rootMoves, currentDepth, window.alpha, window.beta, true);
    if (!result.completed) {
      std::cout << "\n  Time limit reached, keeping depth " << td.completedDepth << "\n";
      break;
    }

    if (window.narrowed && (result.failLow || result.failHigh)) {
      std::cout << " [re-search " << moveToString(result.bestMove) << "]";
      std::cout.flush();
      result = searchRoot(td, rootMoves, currentDepth, ROOT_FULL_ALPHA, ROOT_FULL_BETA, false);
      if (!result.completed) {
        std::cout << "\n  Time limit reached, keeping depth " << td.completedDepth << "\n";
        break;
      }
    }

    td.bestMove = result.bestMove;
    td.bestScore = result.bestScore;
    td.completedDepth = currentDepth;

    uint64_t totalNodes = getNodesSearched();
    uint64_t totalTTHits = getTTHits();
    std::cout << "\n  Depth " << currentDepth << " complete: " << moveToString(td.bestMove)
              << " (score: " << td.bestScore << ", nodes: " << totalNodes
              << ", tt hits: " << totalTTHits << ")\n";
  }

  // Signal helpers to stop and wait
  stopSearch.store(true);
  for (auto& h : helpers) h.join();

  const ThreadData* selected = &threads[0];
  for (const ThreadData& candidate : threads) {
    if (candidate.completedDepth > selected->completedDepth) {
      selected = &candidate;
      continue;
    }

    if (candidate.completedDepth == selected->completedDepth &&
        candidate.bestScore > selected->bestScore) {
      selected = &candidate;
      continue;
    }

    if (candidate.completedDepth == selected->completedDepth &&
        candidate.bestScore == selected->bestScore && candidate.threadId < selected->threadId) {
      selected = &candidate;
    }
  }

  Move finalMove = (selected->bestMove != 0) ? selected->bestMove : rootMoves[0];
  int finalScore = (selected->completedDepth > 0) ? selected->bestScore : 0;
  lastSelectedThreadId = selected->threadId;
  lastSelectedDepth = selected->completedDepth;

  uint64_t totalNodes = getNodesSearched();
  uint64_t totalTTHits = getTTHits();
  std::cout << "Best move: " << moveToString(finalMove) << " (score: " << finalScore
            << ", depth: " << selected->completedDepth;
  if (numThreads > 1) std::cout << ", thread: " << selected->threadId;
  std::cout << ")\n";
  std::cout << "Total nodes: " << totalNodes << ", TT hits: " << totalTTHits << "\n";

  return finalMove;
}

// =============================================================================
// Helper thread: runs independent iterative deepening to populate shared TT
// =============================================================================
// Depth-skip table for helper thread diversity (Stockfish-inspired).
// Each thread uses a different row. On iteration `i`, the thread skips
// the depth if skipTable[threadId % SKIP_ROWS][i % SKIP_COLS] is true.
// This ensures threads are usually searching at different depths.
static constexpr int SKIP_ROWS = 6;
static constexpr int SKIP_COLS = 8;
static constexpr bool skipTable[SKIP_ROWS][SKIP_COLS] = {
    {false, true, false, false, true, false, false, true},   // thread 1
    {false, false, true, false, false, true, false, false},  // thread 2
    {true, false, false, true, false, false, true, false},   // thread 3
    {false, false, true, false, true, false, false, false},  // thread 4
    {false, true, false, false, false, true, false, true},   // thread 5
    {true, false, false, false, true, false, true, false},   // thread 6+
};

AI::RootWindow AI::getRootAspirationWindow(const ThreadData& td, int currentDepth) const {
  RootWindow window{ROOT_FULL_ALPHA, ROOT_FULL_BETA, false};
  if (currentDepth < 5 || td.completedDepth == 0) return window;

  int aspiration = MAIN_ASPIRATION_WINDOW;
  if (td.threadId > 0) {
    switch ((td.threadId - 1) % 3) {
      case 0:
        aspiration = HELPER_WIDE_ASPIRATION_WINDOW;
        break;
      case 1:
        aspiration = HELPER_NARROW_ASPIRATION_WINDOW;
        break;
      case 2:
        if (currentDepth % 3 == 0) return window;
        break;
    }
  }

  window.alpha = td.bestScore - aspiration;
  window.beta = td.bestScore + aspiration;
  window.narrowed = true;
  return window;
}

AI::RootSearchResult AI::searchRoot(ThreadData& td, const MoveList& rootMoves, int currentDepth,
                                    int alpha, int beta, bool reportRootMoves) {
  RootSearchResult result;
  result.bestMove = rootMoves[0];
  int alphaOrig = alpha;

  for (size_t mi = 0; mi < rootMoves.size(); ++mi) {
    if (stopSearch.load(std::memory_order_relaxed) || shouldStop()) {
      stopSearch.store(true, std::memory_order_relaxed);
      return result;
    }

    Move move = rootMoves[mi];
    if (reportRootMoves) {
      std::cout << moveToString(move) << " ";
      std::cout.flush();

      if (moveCallback) {
        moveCallback(move, currentDepth, td.pos);
      }
    }

    td.pos.makeMove(move);
    int score = -negamax(td, currentDepth - 1, -beta, -alpha, 1);
    td.pos.unmakeMove();

    if (stopSearch.load(std::memory_order_relaxed)) {
      return result;
    }

    if (score > result.bestScore) {
      result.bestScore = score;
      result.bestMove = move;
    }

    alpha = std::max(alpha, score);
    if (score >= beta) {
      result.completed = true;
      result.failHigh = true;
      return result;
    }
  }

  result.completed = true;
  if (result.bestScore <= alphaOrig) {
    result.failLow = true;
  }
  return result;
}

void AI::helperThreadSearch(ThreadData& td, const MoveList& rootMoves, int maxDepth) {
  int row = (td.threadId - 1) % SKIP_ROWS;
  MoveList orderedRootMoves = rootMoves;

  for (int currentDepth = 1; currentDepth <= maxDepth; ++currentDepth) {
    if (stopSearch.load(std::memory_order_relaxed)) return;

    // Skip some depths to create diversity — different threads search different depths
    if (currentDepth >= 3 && skipTable[row][(currentDepth - 1) % SKIP_COLS]) {
      continue;
    }

    Move previousBestMove = (td.completedDepth > 0) ? td.bestMove : Move(0);
    ScoredMoveList scoredRootMoves = orderMoves(td, orderedRootMoves, 0, previousBestMove);
    orderedRootMoves.clear();
    for (size_t ri = 0; ri < scoredRootMoves.size(); ++ri) {
      orderedRootMoves.push_back(scoredRootMoves[ri].move);
    }

    RootWindow window = getRootAspirationWindow(td, currentDepth);
    RootSearchResult result =
        searchRoot(td, orderedRootMoves, currentDepth, window.alpha, window.beta, false);
    if (!result.completed) return;

    if (window.narrowed && (result.failLow || result.failHigh)) {
      result =
          searchRoot(td, orderedRootMoves, currentDepth, ROOT_FULL_ALPHA, ROOT_FULL_BETA, false);
      if (!result.completed) return;
    }

    td.bestMove = result.bestMove;
    td.bestScore = result.bestScore;
    td.completedDepth = currentDepth;
  }
}

// =============================================================================
// negamax — the core search function
// =============================================================================
int AI::negamax(ThreadData& td, int depth, int alpha, int beta, int ply, Move excludedMove) {
  td.nodesSearched++;

  prefetchTT(td.pos.hash());

  // Periodic time check (every 1024 nodes)
  if ((td.nodesSearched & 0x3FF) == 0 && shouldStop()) {
    stopSearch.store(true, std::memory_order_relaxed);
    return 0;
  }

  // Draw detection (skip root)
  if (ply > 0) {
    if (td.pos.repetitionCount() >= 2 || td.pos.halfmoveClock() >= 100) {
      int phase = td.pos.getGamePhase();
      int contempt = -CONTEMPT_MAX * (256 - phase) / 256;
      int material =
          td.pos.materialCount(td.pos.sideToMove()) - td.pos.materialCount(~td.pos.sideToMove());
      if (material > CONTEMPT_MATERIAL_THRESHOLD)
        contempt -= CONTEMPT_AHEAD_BONUS;
      else if (material < -CONTEMPT_MATERIAL_THRESHOLD)
        contempt += CONTEMPT_BEHIND_OFFSET;
      return contempt;
    }
  }

  // Syzygy WDL probe (depth >= 2, not root)
  if (ply > 0 && depth >= 2 && Tablebase::available() && Tablebase::canProbe(td.pos) &&
      td.pos.castlingRights() == 0) {
    TBResult wdl = Tablebase::probeWDL(td.pos);
    if (wdl != TB_RESULT_UNKNOWN) {
      int tbScore = Tablebase::wdlToScore(wdl, ply);
      storeTT(td.pos.hash(), depth, tbScore, 0, alpha, beta, ply);
      return tbScore;
    }
  }

  int alphaOrig = alpha;
  HashKey hash = td.pos.hash();
  TTProbeInfo ttInfo;

  if (excludedMove == 0) {
    if (auto score = probeTT(td, hash, depth, alpha, beta, ply, ttInfo)) {
      return *score;
    }
  } else {
    int tmpAlpha = std::numeric_limits<int>::min() + 1;
    int tmpBeta = std::numeric_limits<int>::max() - 1;
    probeTT(td, hash, 0, tmpAlpha, tmpBeta, ply, ttInfo);
  }

  Move ttMove = ttInfo.ttMove;

  // Quiescence search at leaf nodes
  if (depth <= 0) {
    td.pvLength[ply] = 0;
    return quiescence(td, alpha, beta, 0);
  }

  // Adaptive null move pruning
  if (auto score = tryNullMovePruning(td, depth, alpha, beta, ply)) {
    return *score;
  }

  bool isPVNode = (static_cast<long long>(beta) - static_cast<long long>(alpha)) > 1;

  auto pruning = canPrune(td, depth, alpha, beta, isPVNode);
  if (pruning.cutoff) return pruning.score;
  bool futilityPrune = pruning.futilityPrune;

  // Singular Extension
  int singularExtension = 0;
  if (depth >= 8 && ttMove != 0 && excludedMove == 0 && !isPVNode && ttInfo.found &&
      ttInfo.ttDepth >= depth - 3 && ttInfo.ttFlag != UPPERBOUND &&
      std::abs(ttInfo.ttScore) < MATE_BOUND) {
    int singularBeta = ttInfo.ttScore - 2 * depth;
    int singularDepth = (depth - 1) / 2;
    int singularScore = negamax(td, singularDepth, singularBeta - 1, singularBeta, ply, ttMove);
    if (singularScore < singularBeta) {
      singularExtension = 1;
    }
  }

  // IID: if no hash move at PV node, do shallow search to find one
  if (!ttMove && isPVNode && depth >= 4) {
    int iidDepth = depth - 2;
    (void)negamax(td, iidDepth, alpha, beta, ply);
    TTProbeInfo iidInfo;
    int tmpAlpha = alpha, tmpBeta = beta;
    (void)probeTT(td, hash, iidDepth, tmpAlpha, tmpBeta, ply, iidInfo);
    if (iidInfo.ttMove != 0) {
      ttMove = iidInfo.ttMove;
    }
  }

  // Staged move generation via MovePicker
  Move k1 = (ply < 64) ? td.killerMoves[ply][0] : Move(0);
  Move k2 = (ply < 64) ? td.killerMoves[ply][1] : Move(0);

  Move cm = 0;
  const auto& hist = td.pos.getHistory();
  if (!hist.empty()) {
    Move prevMove = hist.back().move;
    cm = td.countermoves[fromSquare(prevMove)][toSquare(prevMove)];
  }

  MovePicker picker(td.pos, ttMove, k1, k2, cm, td.historyTable, excludedMove);

  int maxScore = std::numeric_limits<int>::min();
  Move bestMove = 0;
  td.pvLength[ply] = 0;
  int moveNum = 0;

  Move quietsTried[64];
  int numQuietsTried = 0;

  Move move;
  while ((move = picker.next()) != 0) {
    Square to = toSquare(move);
    bool isCapture = td.pos.pieceAt(to) != NO_PIECE || moveType(move) == EN_PASSANT;
    bool isPromotion = moveType(move) == PROMOTION;
    bool isKillerMove = isKiller(td, move, ply);

    if (futilityPrune && moveNum > 0 && !isCapture && !isPromotion) {
      continue;
    }

    if (depth <= 3 && moveNum >= static_cast<int>(3 + depth * depth) && !isCapture &&
        !isPromotion && !isKillerMove) {
      continue;
    }

    // Lazy legality
    td.pos.makeMove(move);
    Color us = ~td.pos.sideToMove();
    if (td.pos.isAttacked(BB::lsb(td.pos.pieces(us, KING)), td.pos.sideToMove())) {
      td.pos.unmakeMove();
      continue;
    }

    bool givesCheck = td.pos.inCheck();
    int extension = givesCheck ? 1 : 0;
    if (move == ttMove) extension = std::max(extension, singularExtension);

    int newDepth = depth - 1 + extension;

    int score;
    if (moveNum == 0) {
      score = -negamax(td, newDepth, -beta, -alpha, ply + 1);
    } else {
      int reduction = 0;

      if (depth >= 3 && moveNum >= 3 && !isCapture && !givesCheck && !isPromotion) {
        reduction = lmrTable[std::min(depth, 63)][std::min(moveNum, 63)];

        if (isPVNode) reduction--;
        if (isKillerMove) reduction--;
        if (move == cm) reduction--;

        int histScore = td.historyTable[fromSquare(move)][toSquare(move)];
        reduction -= histScore / 5000;

        // Thread-specific LMR variation: odd threads reduce more (aggressive),
        // even helpers reduce less (conservative). Thread 0 (main) is unchanged.
        if (td.threadId > 0) {
          reduction += (td.threadId & 1) ? 1 : -1;
        }

        reduction = std::max(0, std::min(reduction, newDepth - 1));
      }

      score = -negamax(td, newDepth - reduction, -alpha - 1, -alpha, ply + 1);

      if (score > alpha && reduction > 0) {
        score = -negamax(td, newDepth, -alpha - 1, -alpha, ply + 1);
      }

      if (score > alpha && score < beta) {
        score = -negamax(td, newDepth, -beta, -alpha, ply + 1);
      }
    }

    td.pos.unmakeMove();

    if (!isCapture && !isPromotion && numQuietsTried < 64) {
      quietsTried[numQuietsTried++] = move;
    }

    if (score > maxScore) {
      maxScore = score;
      bestMove = move;

      td.pvTable[ply][0] = move;
      for (int i = 0; i < td.pvLength[ply + 1]; ++i) {
        td.pvTable[ply][i + 1] = td.pvTable[ply + 1][i];
      }
      td.pvLength[ply] = td.pvLength[ply + 1] + 1;
    }

    alpha = std::max(alpha, score);

    if (alpha >= beta) {
      if (!isCapture) {
        storeKiller(td, move, ply);
        updateHistory(td, move, depth * depth);

        for (int i = 0; i < numQuietsTried - 1; i++) {
          updateHistory(td, quietsTried[i], -(depth * depth));
        }

        if (!hist.empty()) {
          Move prevMove = hist.back().move;
          td.countermoves[fromSquare(prevMove)][toSquare(prevMove)] = move;
        }
      }
      break;
    }

    moveNum++;
  }

  if (moveNum == 0 && maxScore == std::numeric_limits<int>::min()) {
    if (excludedMove != 0) {
      return alpha;
    }
    if (td.pos.inCheck()) {
      return -MATE_SCORE + ply;
    } else {
      return 0;
    }
  }

  if (excludedMove == 0) {
    storeTT(hash, depth, maxScore, bestMove, alphaOrig, beta, ply);
  }

  return maxScore;
}

// =============================================================================
// Quiescence search
// =============================================================================
int AI::quiescence(ThreadData& td, int alpha, int beta, int qsDepth) {
  td.nodesSearched++;

  if (td.pos.repetitionCount() >= 2) {
    int phase = td.pos.getGamePhase();
    int contempt = -CONTEMPT_MAX * (256 - phase) / 256;
    int material =
        td.pos.materialCount(td.pos.sideToMove()) - td.pos.materialCount(~td.pos.sideToMove());
    if (material > CONTEMPT_MATERIAL_THRESHOLD)
      contempt -= CONTEMPT_AHEAD_BONUS;
    else if (material < -CONTEMPT_MATERIAL_THRESHOLD)
      contempt += CONTEMPT_BEHIND_OFFSET;
    return contempt;
  }

  bool inCheck = td.pos.inCheck();
  int standPat = 0;

  if (!inCheck) {
    standPat = Eval::evaluate(td.pos);
    if (standPat >= beta) return beta;

    constexpr int DELTA_MARGIN = 900;
    if (standPat + DELTA_MARGIN < alpha) return alpha;
    if (alpha < standPat) alpha = standPat;
  }

  MoveList captures;
  if (inCheck) {
    captures = MoveGen::generateLegalMoves(td.pos);
    if (captures.empty()) {
      return -MATE_SCORE + static_cast<int>(td.pos.getHistory().size());
    }
  } else {
    captures = MoveGen::generateCaptures(td.pos);
    if (qsDepth == 0) {
      MoveList checks = MoveGen::generateCheckingMoves(td.pos);
      captures.append(checks.begin(), checks.end());
    }
  }

  ScoredMoveList scoredCaptures;
  for (Move m : captures) {
    scoredCaptures.push_back(scoreMoveWithSEE(td, m, 0, 0));
  }
  std::sort(scoredCaptures.begin(), scoredCaptures.end(),
            [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

  for (const ScoredMove& sm : scoredCaptures) {
    if (sm.seeValue != std::numeric_limits<int>::min() && sm.seeValue < 0) {
      continue;
    }

    Square to = toSquare(sm.move);
    Piece captured = td.pos.pieceAt(to);
    if (captured != NO_PIECE) {
      static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
      if (standPat + pieceValues[typeOf(captured)] + 200 < alpha) {
        continue;
      }
    }

    td.pos.makeMove(sm.move);

    Color qUs = ~td.pos.sideToMove();
    if (td.pos.isAttacked(BB::lsb(td.pos.pieces(qUs, KING)), td.pos.sideToMove())) {
      td.pos.unmakeMove();
      continue;
    }

    int score = -quiescence(td, -beta, -alpha, qsDepth + 1);
    td.pos.unmakeMove();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }

  return alpha;
}

// =============================================================================
// Move scoring
// =============================================================================
ScoredMove AI::scoreMoveWithSEE(const ThreadData& td, Move move, int ply, Move ttMove) {
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
  Piece captured = td.pos.pieceAt(to);

  if (captured != NO_PIECE || moveType(move) == EN_PASSANT) {
    sm.seeValue = td.pos.see(move);
    if (sm.seeValue > 0) {
      sm.score = 20000 + sm.seeValue;
    } else if (sm.seeValue == 0) {
      sm.score = 10000;
    } else {
      sm.score = 5000 + sm.seeValue;
    }
  } else {
    if (ply > 0) {
      const auto& hist = td.pos.getHistory();
      if (!hist.empty()) {
        Move prevMove = hist.back().move;
        Move cm = td.countermoves[fromSquare(prevMove)][toSquare(prevMove)];
        if (move == cm) {
          sm.score = 9500;
        }
      }
    }
    if (isKiller(td, move, ply)) {
      sm.score += 9000;
    }
    sm.score += td.historyTable[from][to];

    const auto& hist = td.pos.getHistory();
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

ScoredMoveList AI::orderMoves(ThreadData& td, const MoveList& moves, int ply, Move ttMove) {
  ScoredMoveList scored;
  for (size_t i = 0; i < moves.size(); ++i) {
    scored.push_back(scoreMoveWithSEE(td, moves[i], ply, ttMove));
  }
  std::sort(scored.begin(), scored.end(),
            [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });
  return scored;
}

// =============================================================================
// History heuristic
// =============================================================================
void AI::updateHistory(ThreadData& td, Move move, int bonus) {
  Square from = fromSquare(move);
  Square to = toSquare(move);

  td.historyTable[from][to] += bonus - td.historyTable[from][to] * std::abs(bonus) / 10000;

  if (td.historyTable[from][to] > 10000) {
    td.historyTable[from][to] = 10000;
  } else if (td.historyTable[from][to] < -10000) {
    td.historyTable[from][to] = -10000;
  }
}

void AI::storeKiller(ThreadData& td, Move move, int ply) {
  if (ply >= 64) return;
  if (td.killerMoves[ply][0] != move) {
    td.killerMoves[ply][1] = td.killerMoves[ply][0];
    td.killerMoves[ply][0] = move;
  }
}

bool AI::isKiller(const ThreadData& td, Move move, int ply) {
  if (ply >= 64) return false;
  return td.killerMoves[ply][0] == move || td.killerMoves[ply][1] == move;
}

// =============================================================================
// Adaptive null move pruning
// =============================================================================
std::optional<int> AI::tryNullMovePruning(ThreadData& td, int depth, int /*alpha*/, int beta,
                                          int ply) {
  bool canDoNullMove = depth >= 3 && !td.pos.inCheck() && ply > 0;

  if (canDoNullMove) {
    Color us = td.pos.sideToMove();
    int material = td.pos.materialCount(us);

    int knights = BB::popCount(td.pos.pieces(us, KNIGHT));
    int bishops = BB::popCount(td.pos.pieces(us, BISHOP));
    int rooks = BB::popCount(td.pos.pieces(us, ROOK));
    int queens = BB::popCount(td.pos.pieces(us, QUEEN));
    int nonPawnPieces = knights + bishops + rooks + queens;

    if (material <= 100 || nonPawnPieces == 0 || (nonPawnPieces == 1 && material < 500)) {
      canDoNullMove = false;
    }
  }

  if (canDoNullMove) {
    int R = 3 + depth / 6;

    // Thread-specific null-move variation
    if (td.threadId > 0) {
      R += (td.threadId % 3 == 1) ? 1 : (td.threadId % 3 == 2) ? -1 : 0;
    }

    int eval = Eval::evaluate(td.pos);
    if (static_cast<long long>(eval) > static_cast<long long>(beta) + 200) R++;

    R = std::min(R, depth - 1);

    td.pos.makeNullMove();
    int nullDepth = std::max(0, depth - 1 - R);
    int score = -negamax(td, nullDepth, -beta, -beta + 1, ply + 1);
    td.pos.unmakeNullMove();

    if (score >= beta) {
      return beta;
    }
  }

  return std::nullopt;
}

// =============================================================================
// Static pruning
// =============================================================================
AI::PruningResult AI::canPrune(ThreadData& td, int depth, int alpha, int beta, bool isPVNode) {
  PruningResult result = {false, 0, false};

  if (td.pos.inCheck()) return result;

  int eval = Eval::evaluate(td.pos);

  if (depth <= 6 && !isPVNode) {
    int rfpMargin = 100 * depth;
    if (eval - rfpMargin >= beta) {
      result.cutoff = true;
      result.score = eval;
      return result;
    }
  }

  if (depth <= 3 && !isPVNode) {
    int razoringMargin = 300 + 150 * depth;
    if (eval + razoringMargin < alpha) {
      int qscore = quiescence(td, alpha, beta, 0);
      if (qscore < alpha) {
        result.cutoff = true;
        result.score = qscore;
        return result;
      }
    }
  }

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
// Transposition table (shared across threads, lockless)
// =============================================================================
std::optional<int> AI::probeTT(ThreadData& td, HashKey hash, int depth, int& alpha, int& beta,
                               int ply, TTProbeInfo& info) {
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

      int score = entry.score;
      if (score > MATE_BOUND)
        score -= ply;
      else if (score < -MATE_BOUND)
        score += ply;
      info.ttScore = static_cast<int16_t>(score);

      if (entry.depth >= depth) {
        td.ttHits++;
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

  info = TTProbeInfo{};
  return std::nullopt;
}

void AI::storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta,
                 int ply) {
  size_t bucketIdx = hash % ttBucketCount;
  uint32_t key32 = static_cast<uint32_t>(hash >> 32);
  TTBucket& bucket = transpositionTable[bucketIdx];

  int adjScore = score;
  if (adjScore > MATE_BOUND)
    adjScore -= ply;
  else if (adjScore < -MATE_BOUND)
    adjScore += ply;

  TTFlag flag;
  if (score <= alphaOrig) {
    flag = UPPERBOUND;
  } else if (score >= beta) {
    flag = LOWERBOUND;
  } else {
    flag = EXACT;
  }

  int replaceIdx = 0;
  int worstPriority = std::numeric_limits<int>::max();

  for (int i = 0; i < TT_BUCKET_SIZE; i++) {
    TTEntry& entry = bucket.entries[i];

    if (entry.isEmpty()) {
      replaceIdx = i;
      break;
    }

    if (entry.key32 == key32) {
      replaceIdx = i;
      break;
    }

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
