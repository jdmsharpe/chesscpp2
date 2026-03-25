#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>

#include "Polyglot.h"
#include "Position.h"
#include "Types.h"

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

  // Time management
  void setTimeLimit(int ms) { timeLimit = ms; }
  bool shouldStop() const;

  // Get statistics from last search
  uint64_t getNodesSearched() const { return nodesSearched; }
  uint64_t getTTHits() const { return ttHits; }

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

  // Syzygy tablebase support
  static bool initTablebases(const std::string& path);
  static void freeTablebases();
  static bool hasTablebases();

 private:
  int depth;
  uint64_t nodesSearched;
  uint64_t ttHits;

  // Time management
  int timeLimit;             // Time limit in milliseconds (0 = no limit)
  uint64_t searchStartTime;  // Start time in milliseconds
  bool stopSearch;

  // Callback for GUI updates
  MoveCallback moveCallback;

  // --- Transposition Table (multi-bucket, packed entries) ---
  static constexpr int MATE_SCORE = 10000;
  static constexpr int MATE_BOUND = 9500;

  enum TTFlag : uint8_t { EXACT = 0, LOWERBOUND = 1, UPPERBOUND = 2 };

  struct TTEntry {
    uint32_t key32 = 0;    // Upper 32 bits of hash
    int16_t score = 0;     // Centipawn score
    Move bestMove = 0;     // Best move (uint16_t)
    int8_t depth = 0;      // Search depth
    uint8_t flagAge = 0;   // Bits 0-1: flag, bits 2-7: age (0-63)

    TTFlag getFlag() const { return TTFlag(flagAge & 3); }
    uint8_t getAge() const { return flagAge >> 2; }
    void setFlagAge(TTFlag f, uint8_t age) {
      flagAge = static_cast<uint8_t>((age << 2) | (f & 3));
    }
    bool isEmpty() const { return key32 == 0 && bestMove == 0; }
  };

  static constexpr int TT_BUCKET_SIZE = 4;
  struct TTBucket {
    TTEntry entries[TT_BUCKET_SIZE];
  };

  // Default 128MB
  static constexpr size_t DEFAULT_TT_SIZE_MB = 128;
  size_t ttBucketCount;
  std::vector<TTBucket> transpositionTable;
  uint8_t ttAge;  // Current search age (0-63, wraps)

  // Info returned from TT probe (for singular extension)
  struct TTProbeInfo {
    Move ttMove = 0;
    int16_t ttScore = 0;
    int8_t ttDepth = -1;
    TTFlag ttFlag = EXACT;
    bool found = false;
  };

  // --- LMR (Late Move Reduction) table ---
  static int lmrTable[64][64];  // [depth][moveNum]
  static bool lmrInitialized;
  static void initLMR();

  // Killer moves (non-capture moves that caused cutoffs)
  static const int MAX_KILLERS = 2;
  std::array<std::array<Move, MAX_KILLERS>, 64> killerMoves;  // [ply][slot]

  // History heuristic (move history scores)
  std::array<std::array<int, 64>, 64> historyTable;  // [from][to]

  // Principal Variation (PV) storage
  std::array<std::array<Move, 64>, 64> pvTable;  // [ply][index]
  std::array<int, 64> pvLength;                  // PV length at each ply

  // Countermove heuristic (moves that refute other moves)
  std::array<std::array<Move, 64>, 64>
      countermoves;  // [from][to] -> refutation

  // Opening book: FEN -> list of good moves (ordered by preference)
  std::unordered_map<std::string, std::vector<Move>> openingBook;

  // Polyglot opening book
  PolyglotBook polyglotBook;

  // Minimax with alpha-beta pruning
  // excludedMove: for singular extension searches (skip this move)
  int negamax(Position& pos, int depth, int alpha, int beta, int ply,
              Move excludedMove = 0);

  // Quiescence search for tactical positions
  int quiescence(Position& pos, int alpha, int beta, int qsDepth = 0);

  // Move ordering for root moves (uses full scoring)
  std::vector<ScoredMove> orderMoves(Position& pos,
                                     const std::vector<Move>& moves, int ply,
                                     Move ttMove = 0);
  ScoredMove scoreMoveWithSEE(const Position& pos, Move move, int ply,
                              Move ttMove);

  // History heuristic: bonus for cutoff moves, malus for failed quiet moves
  void updateHistory(Move move, int bonus);

  // Store killer move
  void storeKiller(Move move, int ply);

  // Check if move is killer
  bool isKiller(Move move, int ply) const;

  // Search helper: attempt null move pruning (adaptive R)
  std::optional<int> tryNullMovePruning(Position& pos, int depth, int alpha,
                                        int beta, int ply);

  // Search helper: check static pruning conditions
  struct PruningResult {
    bool cutoff;         // true = return score immediately
    int score;           // only valid if cutoff == true
    bool futilityPrune;  // true = skip quiet moves in move loop
  };
  PruningResult canPrune(Position& pos, int depth, int alpha, int beta,
                         bool isPVNode);

  // TT prefetch: issue cache line load ahead of probe
  void prefetchTT(HashKey hash) const {
    size_t bucketIdx = hash % ttBucketCount;
    __builtin_prefetch(&transpositionTable[bucketIdx], 0, 1);
  }

  // TT probe: returns cutoff score or nullopt. Populates info struct.
  std::optional<int> probeTT(HashKey hash, int depth, int& alpha, int& beta,
                             int ply, TTProbeInfo& info);

  // TT store with mate score adjustment
  void storeTT(HashKey hash, int depth, int score, Move bestMove,
               int alphaOrig, int beta, int ply);
};
