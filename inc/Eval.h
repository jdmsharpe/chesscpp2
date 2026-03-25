#pragma once

#include "Position.h"

namespace Eval {
// Evaluate position from the perspective of the side to move.
// Positive = side to move is better. Units: centipawns.
[[nodiscard]] int evaluate(const Position& pos);

// Exposed for testing — returns raw king safety score for the given side.
// Positive = safer king. Negative = king under attack.
[[nodiscard]] int kingSafetyForTest(const Position& pos, Color c);

// Exposed for testing — returns king-pawn proximity score for the given side.
[[nodiscard]] int kingPawnProximityForTest(const Position& pos, Color c);

// Exposed for testing — returns king centralization bonus for the given side.
[[nodiscard]] int kingCentralizationForTest(const Position& pos, Color c);
}  // namespace Eval
