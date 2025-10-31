#pragma once

#include "Types.h"

// Magic bitboards for sliding piece move generation
// This provides very fast lookups for rook and bishop attacks
namespace Magic {

// Initialize magic bitboard tables (must be called before use)
void init();

// Get rook attacks for a square with given occupancy
Bitboard rookAttacks(Square sq, Bitboard occupied);

// Get bishop attacks for a square with given occupancy
Bitboard bishopAttacks(Square sq, Bitboard occupied);

// Get queen attacks (combination of rook and bishop)
inline Bitboard queenAttacks(Square sq, Bitboard occupied) {
  return rookAttacks(sq, occupied) | bishopAttacks(sq, occupied);
}

// Get attacks between two squares (for checking pins and discovered attacks)
Bitboard between(Square sq1, Square sq2);
Bitboard line(Square sq1, Square sq2);

// Check if squares are aligned (on same rank, file, or diagonal)
bool aligned(Square sq1, Square sq2, Square sq3);

}  // namespace Magic
