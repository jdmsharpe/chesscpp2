#pragma once

#include "Position.h"
#include "Types.h"

#include <vector>

// Fast move generation using bitboards
namespace MoveGen {

// Generate all legal moves
[[nodiscard]] std::vector<Move> generateLegalMoves(Position& pos);

// Generate all pseudo-legal moves (may leave king in check)
[[nodiscard]] std::vector<Move> generatePseudoLegalMoves(const Position& pos);

// Generate only captures
[[nodiscard]] std::vector<Move> generateCaptures(Position& pos);

// Generate non-capture moves that give check (for quiescence search)
// Uses the fallback approach: generates pseudo-legal non-captures,
// makes each move, checks if it gives check, filters for legality.
[[nodiscard]] std::vector<Move> generateCheckingMoves(Position& pos);

// Check if a move is legal
[[nodiscard]] bool isLegal(Position& pos, Move move);

// Count all legal moves (for perft testing)
[[nodiscard]] uint64_t perft(Position& pos, int depth);

// Perft with detailed output
void perftDivide(Position& pos, int depth);

}  // namespace MoveGen
