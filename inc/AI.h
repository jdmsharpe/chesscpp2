#pragma once

#include "Polyglot.h"
#include "Position.h"
#include "Types.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Per-thread search state (thread-local, no sharing needed)
struct ThreadData {
  int threadId = 0;
  Position pos;

  // Search heuristics
  static const int MAX_KILLERS = 2;
  std::array<std::array<Move, MAX_KILLERS>, 64> killerMoves{};
  std::array<std::array<int, 64>, 64> historyTable{};
  std::array<std::array<Move, 64>, 64> countermoves{};
  std::array<std::array<Move, 64>, 64> pvTable{};
  std::array<int, 64> pvLength{};

  // Results
  Move bestMove = 0;
  int bestScore = 0;
  uint64_t nodesSearched = 0;
  uint64_t ttHits = 0;

  void clear() {
    killerMoves = {};
    historyTable = {};
    countermoves = {};
    pvTable = {};
    pvLength = {};
    bestMove = 0;
    bestScore = 0;
    nodesSearched = 0;
    ttHits = 0;
  }
};

// AI engine with minimax + alpha-beta pruning + transposition table
class AI {
 public:
  AI(int depth = 6);

  // Find the best move for the current position
  Move findBestMove(Position& pos);

  // Find the best move with time limit (milliseconds)
  // If timeMs = 0, uses depth-based search
  Move findBestMove(Position& pos, int timeMs);

  // Set search depth
  void setDepth(int d) { depth = d; }
  int getDepth() const { return depth; }

  // Thread count
  void setThreads(int n) { numThreads = std::max(1, n); }
  int getThreads() const { return numThreads; }

  // Time management
  void setTimeLimit(int ms) { timeLimit = ms; }
  bool shouldStop() const;

  // Get statistics from last search (sum across all threads)
  uint64_t getNodesSearched() const;
  uint64_t getTTHits() const;

  // Resize transposition table (in MB)
  void resizeTT(size_t mb) {
    ttBucketCount = (mb * 1024 * 1024) / sizeof(TTBucket);
    transpositionTable.assign(ttBucketCount, TTBucket{});
    ttAge = 0;
  }

  // Clear transposition table
  void clearTT() {
    std::fill(transpositionTable.begin(), transpositionTable.end(), TTBucket{});
    ttAge = 0;
  }

  // Set callback for move updates (for GUI display)
  // Callback receives: (move, depth, position)
  using MoveCallback = std::function<void(Move, int, const Position&)>;
  void setMoveCallback(MoveCallback callback) { moveCallback = callback; }

  // Opening book (text format)
  void loadOpeningBook(const std::string& filename);
  Move probeOpeningBook(const Position& pos);
  bool hasOpeningBook() const;

  // Polyglot opening book (.bin format)
  bool loadPolyglotBook(const std::string& filename);
  Move probePolyglotBook(const Position& pos);
  bool hasPolyglotBook() const;

  // Book usage controls
  void setOwnBook(bool enabled) { useOwnBook = enabled; }
  bool getOwnBook() const { return useOwnBook; }
  void setBookDepth(int maxMove) { bookMoveLimit = maxMove; }
  int getBookDepth() const { return bookMoveLimit; }

  // Syzygy tablebase support
  static bool initTablebases(const std::string& path);
  static void freeTablebases();
  static bool hasTablebases();

 private:
  int depth;
  int numThreads = 1;

  // Time management
  int timeLimit;             // Time limit in milliseconds (0 = no limit)
  uint64_t searchStartTime;  // Start time in milliseconds
  std::atomic<bool> stopSearch{false};

  // Callback for GUI updates
  MoveCallback moveCallback;

  // Thread pool
  std::vector<ThreadData> threads;

  // --- Contempt (phase-scaled draw score) ---
  static constexpr int CONTEMPT_MAX = 15;
  static constexpr int CONTEMPT_AHEAD_BONUS = 15;
  static constexpr int CONTEMPT_BEHIND_OFFSET = 10;
  static constexpr int CONTEMPT_MATERIAL_THRESHOLD = 200;

  // --- Transposition Table (multi-bucket, packed entries) ---
  // Shared across all threads (lockless: key verification catches torn reads)
  static constexpr int MATE_SCORE = 10000;
  static constexpr int MATE_BOUND = 9500;

  enum TTFlag : uint8_t { EXACT = 0, LOWERBOUND = 1, UPPERBOUND = 2 };

  struct TTEntry {
    uint32_t key32 = 0;
    int16_t score = 0;
    Move bestMove = 0;
    int8_t depth = 0;
    uint8_t flagAge = 0;

    TTFlag getFlag() const { return TTFlag(flagAge & 3); }
    uint8_t getAge() const { return flagAge >> 2; }
    void setFlagAge(TTFlag f, uint8_t age) { flagAge = static_cast<uint8_t>((age << 2) | (f & 3)); }
    bool isEmpty() const { return key32 == 0 && bestMove == 0; }
  };

  static constexpr int TT_BUCKET_SIZE = 4;
  struct TTBucket {
    TTEntry entries[TT_BUCKET_SIZE];
  };

  static constexpr size_t DEFAULT_TT_SIZE_MB = 128;
  size_t ttBucketCount;
  std::vector<TTBucket> transpositionTable;
  uint8_t ttAge;

  struct TTProbeInfo {
    Move ttMove = 0;
    int16_t ttScore = 0;
    int8_t ttDepth = -1;
    TTFlag ttFlag = EXACT;
    bool found = false;
  };

  // --- LMR table ---
  static int lmrTable[64][64];
  static bool lmrInitialized;
  static void initLMR();

  // Opening book: FEN -> list of good moves
  std::unordered_map<std::string, std::vector<Move>> openingBook;

  // Polyglot opening book
  PolyglotBook polyglotBook;

  // Book usage controls
  bool useOwnBook = true;
  int bookMoveLimit = 0;

  // --- Search functions (all take ThreadData& for thread-local state) ---
  int negamax(ThreadData& td, int depth, int alpha, int beta, int ply, Move excludedMove = 0);
  int quiescence(ThreadData& td, int alpha, int beta, int qsDepth = 0);

  // Move ordering for root moves
  ScoredMoveList orderMoves(ThreadData& td, const MoveList& moves, int ply, Move ttMove = 0);
  ScoredMove scoreMoveWithSEE(const ThreadData& td, Move move, int ply, Move ttMove);

  // History heuristic
  static void updateHistory(ThreadData& td, Move move, int bonus);

  // Killer moves
  static void storeKiller(ThreadData& td, Move move, int ply);
  static bool isKiller(const ThreadData& td, Move move, int ply);

  // Search helpers
  std::optional<int> tryNullMovePruning(ThreadData& td, int depth, int alpha, int beta, int ply);

  struct PruningResult {
    bool cutoff;
    int score;
    bool futilityPrune;
  };
  PruningResult canPrune(ThreadData& td, int depth, int alpha, int beta, bool isPVNode);

  // TT access (shared, lockless)
  void prefetchTT(HashKey hash) const {
    size_t bucketIdx = hash % ttBucketCount;
    __builtin_prefetch(&transpositionTable[bucketIdx], 0, 1);
  }
  std::optional<int> probeTT(ThreadData& td, HashKey hash, int depth, int& alpha, int& beta,
                             int ply, TTProbeInfo& info);
  void storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta, int ply);

  // Lazy SMP: helper thread entry point
  void helperThreadSearch(ThreadData& td, const MoveList& rootMoves, int maxDepth);
};
