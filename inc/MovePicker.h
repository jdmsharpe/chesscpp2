#pragma once

#include "MoveGen.h"
#include "Position.h"
#include "Types.h"

#include <array>

// Staged move generation: yields pseudo-legal moves in priority order.
// Caller is responsible for legality checking via makeMove + isAttacked.
//
// Stage order:
//   1. TT move
//   2. Good captures + promotions (SEE >= 0)
//   3. Killer moves (quiet moves that caused cutoffs at this ply)
//   4. Countermove (quiet move that refuted the previous move)
//   5. Quiet moves (sorted by history heuristic)
//   6. Bad captures (SEE < 0)
class MovePicker {
 public:
  MovePicker(Position& pos, Move ttMove, Move killer1, Move killer2, Move countermove,
             const std::array<std::array<int, 64>, 64>& history, Move excludedMove = 0);

  // Returns next pseudo-legal move, or 0 when exhausted.
  Move next();

 private:
  enum Stage {
    STAGE_TT,
    STAGE_GENERATE,
    STAGE_GOOD_CAPTURES,
    STAGE_KILLER_1,
    STAGE_KILLER_2,
    STAGE_COUNTERMOVE,
    STAGE_QUIETS,
    STAGE_BAD_CAPTURES,
    STAGE_DONE
  };

  void generateAndScore();
  Move pickBest(ScoredMoveList& list, size_t& startIdx);
  bool isInMoveList(Move m) const;

  Position& pos;
  Move ttMove, excludedMove;
  Move killer1, killer2, countermove;
  const std::array<std::array<int, 64>, 64>& history;
  Stage stage;

  ScoredMoveList goodCaptures;
  ScoredMoveList badCaptures;
  ScoredMoveList quietMoves;
  size_t goodCapIdx = 0, badCapIdx = 0, quietIdx = 0;

  // Stored pseudo-legal moves for killer/countermove validation
  MoveList pseudoLegal;
};
