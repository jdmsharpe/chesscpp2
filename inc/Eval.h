#pragma once

#include "Position.h"

namespace Eval {
// Evaluate position from the perspective of the side to move.
// Positive = side to move is better. Units: centipawns.
int evaluate(const Position& pos);
}  // namespace Eval
