#pragma once

#include <string>

#include "Types.h"

// Bitboard namespace with core operations
namespace BB {

// Bitboard constants
constexpr Bitboard EMPTY = 0ULL;
constexpr Bitboard ALL = ~0ULL;

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = FILE_A_BB << 1;
constexpr Bitboard FILE_C_BB = FILE_A_BB << 2;
constexpr Bitboard FILE_D_BB = FILE_A_BB << 3;
constexpr Bitboard FILE_E_BB = FILE_A_BB << 4;
constexpr Bitboard FILE_F_BB = FILE_A_BB << 5;
constexpr Bitboard FILE_G_BB = FILE_A_BB << 6;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;

constexpr Bitboard RANK_1_BB = 0xFFULL;
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_3_BB = RANK_1_BB << 16;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_6_BB = RANK_1_BB << 40;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

// Set/clear/test bits
constexpr Bitboard squareBB(Square sq) { return 1ULL << sq; }

constexpr bool testBit(Bitboard bb, Square sq) { return bb & squareBB(sq); }

constexpr Bitboard setBit(Bitboard bb, Square sq) { return bb | squareBB(sq); }

constexpr Bitboard clearBit(Bitboard bb, Square sq) {
  return bb & ~squareBB(sq);
}

// Population count (number of set bits)
inline int popCount(Bitboard bb) { return __builtin_popcountll(bb); }

// Find least significant bit (LSB)
inline Square lsb(Bitboard bb) { return Square(__builtin_ctzll(bb)); }

// Find most significant bit (MSB)
inline Square msb(Bitboard bb) { return Square(63 - __builtin_clzll(bb)); }

// Pop LSB and return it
inline Square popLsb(Bitboard& bb) {
  Square sq = lsb(bb);
  bb &= bb - 1;  // Clear LSB
  return sq;
}

// Shift operations
template <Color C>
constexpr Bitboard pawnPush(Bitboard bb) {
  return C == WHITE ? bb << 8 : bb >> 8;
}

template <Color C>
constexpr Bitboard pawnDoublePush(Bitboard bb) {
  return C == WHITE ? bb << 16 : bb >> 16;
}

template <Color C>
constexpr Bitboard pawnAttackWest(Bitboard bb) {
  return C == WHITE ? (bb & ~FILE_A_BB) << 7 : (bb & ~FILE_A_BB) >> 9;
}

template <Color C>
constexpr Bitboard pawnAttackEast(Bitboard bb) {
  return C == WHITE ? (bb & ~FILE_H_BB) << 9 : (bb & ~FILE_H_BB) >> 7;
}

template <Color C>
constexpr Bitboard pawnAttacks(Bitboard bb) {
  return pawnAttackWest<C>(bb) | pawnAttackEast<C>(bb);
}

// File and rank helpers
constexpr Bitboard fileBB(Square sq) { return FILE_A_BB << fileOf(sq); }

constexpr Bitboard rankBB(Square sq) { return RANK_1_BB << (rankOf(sq) * 8); }

// Print bitboard (for debugging)
std::string toString(Bitboard bb);

// Initialize lookup tables
void init();

// Attack lookup tables (initialized by init())
extern Bitboard PawnAttacks[2][64];
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];

// Between bitboards (squares between two squares)
extern Bitboard BetweenBB[64][64];
inline Bitboard betweenBB(Square sq1, Square sq2) {
  return BetweenBB[sq1][sq2];
}

// Pawn attack lookup
inline Bitboard pawnAttacks(Color c, Square sq) { return PawnAttacks[c][sq]; }

inline Bitboard knightAttacks(Square sq) { return KnightAttacks[sq]; }

inline Bitboard kingAttacks(Square sq) { return KingAttacks[sq]; }

}  // namespace BB
