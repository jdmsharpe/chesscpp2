#pragma once

#include <array>
#include <chrono>
#include <functional>
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

  // Clear transposition table
  void clearTT() {
    std::fill(transpositionTable.begin(), transpositionTable.end(), TTEntry{});
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

  // Transposition table entry
  enum TTFlag { EXACT, LOWERBOUND, UPPERBOUND };
  struct TTEntry {
    HashKey key;
    int depth;
    int score;
    TTFlag flag;
    Move bestMove;
    uint8_t age;  // For aging entries
  };

  // Fixed-size transposition table (128MB default)
  static constexpr size_t TT_SIZE_MB = 128;
  static constexpr size_t TT_SIZE =
      (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
  std::vector<TTEntry> transpositionTable;
  uint8_t ttAge;  // Current search age

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
  int negamax(Position& pos, int depth, int alpha, int beta, int ply);

  // Quiescence search for tactical positions
  int quiescence(Position& pos, int alpha, int beta, int qsDepth = 0);

  // Position evaluation
  int evaluate(const Position& pos);

  // Move ordering for better pruning
  void orderMoves(Position& pos, std::vector<Move>& moves, int ply,
                  Move ttMove = 0);
  int getMoveScore(const Position& pos, Move move, int ply, Move ttMove);

  // Update history heuristic
  void updateHistory(Move move, int depth);

  // Store killer move
  void storeKiller(Move move, int ply);

  // Check if move is killer
  bool isKiller(Move move, int ply) const;

  // Evaluation helper functions
  int evaluatePawnStructure(const Position& pos, Color c) const;
  int evaluateKingSafety(const Position& pos, Color c) const;
  int evaluateMobility(const Position& pos, Color c);
  int evaluateDevelopment(const Position& pos, Color c) const;
  int evaluateRooks(const Position& pos, Color c) const;
  int evaluateBishops(const Position& pos, Color c) const;
  int evaluateKnights(const Position& pos, Color c) const;
  int getGamePhase(const Position& pos) const;
};
