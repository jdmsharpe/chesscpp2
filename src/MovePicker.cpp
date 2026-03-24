#include "MovePicker.h"

#include <algorithm>
#include <limits>

MovePicker::MovePicker(Position& pos, Move ttMove, Move killer1, Move killer2,
                       Move countermove,
                       const std::array<std::array<int, 64>, 64>& history,
                       Move excludedMove)
    : pos(pos),
      ttMove(ttMove),
      excludedMove(excludedMove),
      killer1(killer1),
      killer2(killer2),
      countermove(countermove),
      history(history),
      stage(ttMove ? STAGE_TT : STAGE_GENERATE) {}

Move MovePicker::next() {
  switch (stage) {
    case STAGE_TT:
      stage = STAGE_GENERATE;
      if (ttMove && ttMove != excludedMove) return ttMove;
      [[fallthrough]];

    case STAGE_GENERATE:
      generateAndScore();
      stage = STAGE_GOOD_CAPTURES;
      [[fallthrough]];

    case STAGE_GOOD_CAPTURES:
      while (goodCapIdx < goodCaptures.size()) {
        Move m = pickBest(goodCaptures, goodCapIdx);
        if (m != ttMove && m != excludedMove) return m;
      }
      stage = STAGE_KILLER_1;
      [[fallthrough]];

    case STAGE_KILLER_1:
      stage = STAGE_KILLER_2;
      if (killer1 && killer1 != ttMove && killer1 != excludedMove &&
          isInMoveList(killer1)) {
        // Killers are quiet moves only
        if (pos.pieceAt(toSquare(killer1)) == NO_PIECE &&
            moveType(killer1) != EN_PASSANT) {
          return killer1;
        }
      }
      [[fallthrough]];

    case STAGE_KILLER_2:
      stage = STAGE_COUNTERMOVE;
      if (killer2 && killer2 != ttMove && killer2 != killer1 &&
          killer2 != excludedMove && isInMoveList(killer2)) {
        if (pos.pieceAt(toSquare(killer2)) == NO_PIECE &&
            moveType(killer2) != EN_PASSANT) {
          return killer2;
        }
      }
      [[fallthrough]];

    case STAGE_COUNTERMOVE:
      stage = STAGE_QUIETS;
      if (countermove && countermove != ttMove && countermove != killer1 &&
          countermove != killer2 && countermove != excludedMove &&
          isInMoveList(countermove)) {
        if (pos.pieceAt(toSquare(countermove)) == NO_PIECE &&
            moveType(countermove) != EN_PASSANT) {
          return countermove;
        }
      }
      [[fallthrough]];

    case STAGE_QUIETS:
      while (quietIdx < quietMoves.size()) {
        Move m = pickBest(quietMoves, quietIdx);
        if (m != ttMove && m != killer1 && m != killer2 &&
            m != countermove && m != excludedMove) {
          return m;
        }
      }
      stage = STAGE_BAD_CAPTURES;
      [[fallthrough]];

    case STAGE_BAD_CAPTURES:
      while (badCapIdx < badCaptures.size()) {
        Move m = pickBest(badCaptures, badCapIdx);
        if (m != ttMove && m != excludedMove) return m;
      }
      stage = STAGE_DONE;
      [[fallthrough]];

    case STAGE_DONE:
      return 0;
  }
  return 0;
}

void MovePicker::generateAndScore() {
  pseudoLegal = MoveGen::generatePseudoLegalMoves(pos);

  for (Move m : pseudoLegal) {
    Square to = toSquare(m);
    Square from = fromSquare(m);
    bool isCapture =
        pos.pieceAt(to) != NO_PIECE || moveType(m) == EN_PASSANT;
    bool isPromotion = moveType(m) == PROMOTION;

    ScoredMove sm;
    sm.move = m;
    sm.seeValue = std::numeric_limits<int>::min();

    if (isCapture) {
      sm.seeValue = pos.see(m);
      if (isPromotion) {
        // Capture-promotions are always high priority
        sm.score = 30000 + sm.seeValue;
        goodCaptures.push_back(sm);
      } else if (sm.seeValue >= 0) {
        sm.score = 20000 + sm.seeValue;
        goodCaptures.push_back(sm);
      } else {
        sm.score = sm.seeValue;
        badCaptures.push_back(sm);
      }
    } else if (isPromotion) {
      // Non-capture promotions (push to promo rank)
      sm.score = 30000;
      goodCaptures.push_back(sm);
    } else {
      // Quiet move — scored by history
      sm.score = history[from][to];
      quietMoves.push_back(sm);
    }
  }
}

Move MovePicker::pickBest(std::vector<ScoredMove>& list, size_t& startIdx) {
  // Selection sort: find best from startIdx onward, swap to front
  size_t bestIdx = startIdx;
  for (size_t i = startIdx + 1; i < list.size(); i++) {
    if (list[i].score > list[bestIdx].score) {
      bestIdx = i;
    }
  }
  if (bestIdx != startIdx) {
    std::swap(list[startIdx], list[bestIdx]);
  }
  return list[startIdx++].move;
}

bool MovePicker::isInMoveList(Move m) const {
  for (Move pm : pseudoLegal) {
    if (pm == m) return true;
  }
  return false;
}
