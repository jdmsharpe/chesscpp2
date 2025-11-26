#ifndef TABLEBASE_H
#define TABLEBASE_H

#include <string>

#include "Position.h"
#include "Types.h"

// Syzygy tablebase result values
enum TBResult {
  TB_RESULT_LOSS = 0,
  TB_RESULT_BLESSED_LOSS = 1,  // Loss but 50-move draw
  TB_RESULT_DRAW = 2,
  TB_RESULT_CURSED_WIN = 3,  // Win but 50-move draw
  TB_RESULT_WIN = 4,
  TB_RESULT_UNKNOWN = 5  // Probe failed
};

// Tablebase probe result with move info
struct TBProbeResult {
  TBResult wdl;      // Win/Draw/Loss result
  int dtz;           // Distance to zeroing move (pawn push or capture)
  Move bestMove;     // Best move according to tablebase
  bool success;      // Whether probe succeeded
};

class Tablebase {
 public:
  // Initialize tablebases from path (semicolon-separated on Windows, colon on Unix)
  static bool init(const std::string& path);

  // Free tablebase resources
  static void free();

  // Check if tablebases are available
  static bool available();

  // Get the maximum number of pieces supported by loaded tablebases
  static int maxPieces();

  // Probe WDL (Win/Draw/Loss) - thread-safe, use during search
  // Returns TB_RESULT_UNKNOWN if probe fails
  static TBResult probeWDL(const Position& pos);

  // Probe root position with DTZ info - NOT thread-safe, use at root only
  // Returns full information including best move
  static TBProbeResult probeRoot(const Position& pos);

  // Convert WDL result to a score in centipawns
  // Uses TB scoring: Win = +TB_WIN_SCORE, Loss = -TB_WIN_SCORE, Draw = 0
  static int wdlToScore(TBResult wdl, int ply = 0);

  // Check if position is eligible for tablebase probe
  // (piece count <= max, no castling rights for WDL)
  static bool canProbe(const Position& pos);

  // Tablebase score constants
  static constexpr int TB_WIN_SCORE = 10000;
  static constexpr int TB_CURSED_WIN_SCORE = 1;
  static constexpr int TB_BLESSED_LOSS_SCORE = -1;
  static constexpr int TB_LOSS_SCORE = -10000;

 private:
  static bool initialized;
  static int largestPieces;
};

#endif  // TABLEBASE_H
