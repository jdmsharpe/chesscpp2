#pragma once

#include "Position.h"
#include "Types.h"

#include <vector>

// Fast move generation using bitboards
namespace MoveGen {

// Generate all legal moves
std::vector<Move> generateLegalMoves(Position& pos);

// Generate all pseudo-legal moves (may leave king in check)
std::vector<Move> generatePseudoLegalMoves(const Position& pos);

// Generate only captures
std::vector<Move> generateCaptures(Position& pos);

// Generate non-capture moves that give check (for quiescence search)
// Uses the fallback approach: generates pseudo-legal non-captures,
// makes each move, checks if it gives check, filters for legality.
std::vector<Move> generateCheckingMoves(Position& pos);

// Check if a move is legal
bool isLegal(Position& pos, Move move);

// Count all legal moves (for perft testing)
uint64_t perft(Position& pos, int depth);

// Perft with detailed output
void perftDivide(Position& pos, int depth);

}  // namespace MoveGen
