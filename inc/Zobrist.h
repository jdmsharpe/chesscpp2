#pragma once

#include "Types.h"

// Zobrist hashing for position keys
namespace Zobrist {

// Zobrist random numbers
extern HashKey psq[12][64];   // [piece][square]
extern HashKey enpassant[8];  // [file]
extern HashKey castling[16];  // [castling rights]
extern HashKey sideToMove;    // side to move

// Initialize zobrist keys
void init();

}  // namespace Zobrist
