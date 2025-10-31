#include "Magic.h"

#include <random>

#include "Bitboard.h"

namespace Magic {

// Magic bitboard structure
struct MagicEntry {
  Bitboard mask;      // Relevant occupancy mask
  Bitboard magic;     // Magic number
  Bitboard* attacks;  // Pointer to attack table
  int shift;          // Shift for index calculation
};

// Magic tables
static MagicEntry RookMagics[64];
static MagicEntry BishopMagics[64];

// Attack tables storage
static Bitboard RookTable[102400];
static Bitboard BishopTable[5248];

// Between and line tables
static Bitboard BetweenTable[64][64];
static Bitboard LineTable[64][64];

// Direction offsets for sliding pieces
constexpr int RookDirections[4] = {8, -8, 1, -1};    // N, S, E, W
constexpr int BishopDirections[4] = {9, 7, -7, -9};  // NE, NW, SW, SE

// Generate sliding attacks (slow, used for initialization)
Bitboard slidingAttacks(Square sq, Bitboard occupied, const int directions[4]) {
  Bitboard attacks = BB::EMPTY;

  for (int i = 0; i < 4; ++i) {
    int dir = directions[i];
    int to = sq;
    while (true) {
      to += dir;

      // Check if we went off the board
      if (to < 0 || to >= 64) break;

      // Check for edge wrapping (horizontal)
      if (dir == 1 && fileOf(to) == FILE_A) break;
      if (dir == -1 && fileOf(to) == FILE_H) break;

      // Check for edge wrapping (diagonal)
      if (dir == 9 && fileOf(to) == FILE_A) break;   // NE wrapped
      if (dir == 7 && fileOf(to) == FILE_H) break;   // NW wrapped
      if (dir == -7 && fileOf(to) == FILE_A) break;  // SW wrapped
      if (dir == -9 && fileOf(to) == FILE_H) break;  // SE wrapped

      attacks |= BB::squareBB(Square(to));

      // Stop if we hit an occupied square
      if (occupied & BB::squareBB(Square(to))) break;
    }
  }

  return attacks;
}

// Generate relevant occupancy mask (excludes edges for better magic numbers)
Bitboard relevantOccupancy(Square sq, const int directions[4]) {
  Bitboard mask = BB::EMPTY;

  for (int i = 0; i < 4; ++i) {
    int dir = directions[i];
    int to = sq;
    while (true) {
      to += dir;

      // Check bounds
      if (to < 0 || to >= 64) break;

      // Check for wrapping around board edges (horizontal)
      if (dir == 1 && fileOf(to) == FILE_A) break;   // Wrapped east->west
      if (dir == -1 && fileOf(to) == FILE_H) break;  // Wrapped west->east

      // Check for edge wrapping (diagonal)
      if (dir == 9 && fileOf(to) == FILE_A) break;   // NE wrapped
      if (dir == 7 && fileOf(to) == FILE_H) break;   // NW wrapped
      if (dir == -7 && fileOf(to) == FILE_A) break;  // SW wrapped
      if (dir == -9 && fileOf(to) == FILE_H) break;  // SE wrapped

      // Check if we've reached the edge in this specific direction
      bool atEdge = false;
      if (dir == 8 && rankOf(to) == RANK_8)
        atEdge = true;  // Going north, reached rank 8
      if (dir == -8 && rankOf(to) == RANK_1)
        atEdge = true;  // Going south, reached rank 1
      if (dir == 1 && fileOf(to) == FILE_H)
        atEdge = true;  // Going east, reached file H
      if (dir == -1 && fileOf(to) == FILE_A)
        atEdge = true;  // Going west, reached file A
      if (dir == 9 && (rankOf(to) == RANK_8 || fileOf(to) == FILE_H))
        atEdge = true;  // NE edge
      if (dir == 7 && (rankOf(to) == RANK_8 || fileOf(to) == FILE_A))
        atEdge = true;  // NW edge
      if (dir == -7 && (rankOf(to) == RANK_1 || fileOf(to) == FILE_H))
        atEdge = true;  // SW edge
      if (dir == -9 && (rankOf(to) == RANK_1 || fileOf(to) == FILE_A))
        atEdge = true;  // SE edge

      // Include in mask if not at the edge (edge squares don't block for magic
      // purposes)
      if (!atEdge) {
        mask |= BB::squareBB(Square(to));
      }

      // Stop when we hit the edge
      if (atEdge) break;
    }
  }

  return mask;
}

// Index calculation for magic bitboards
inline int magicIndex(const MagicEntry& entry, Bitboard occupied) {
  return ((occupied & entry.mask) * entry.magic) >> entry.shift;
}

// Fixed magic numbers for rooks (found by magic number generators)
constexpr Bitboard RookMagicNumbers[64] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL,
    0x0080040800100080ULL, 0x0080020400080080ULL, 0x0080010200040080ULL,
    0x0080008001000200ULL, 0x0080002040800100ULL, 0x0000800020400080ULL,
    0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
    0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL,
    0x0000800040800100ULL, 0x0000208000400080ULL, 0x0000404000201000ULL,
    0x0000808010002000ULL, 0x0000808008001000ULL, 0x0000808004000800ULL,
    0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020000408104ULL,
    0x0000208080004000ULL, 0x0000200040005000ULL, 0x0000100080200080ULL,
    0x0000080080100080ULL, 0x0000040080080080ULL, 0x0000020080040080ULL,
    0x0000010080800200ULL, 0x0000800080004100ULL, 0x0000204000800080ULL,
    0x0000200040401000ULL, 0x0000100080802000ULL, 0x0000080080801000ULL,
    0x0000040080800800ULL, 0x0000020080800400ULL, 0x0000020001010004ULL,
    0x0000800040800100ULL, 0x0000204000808000ULL, 0x0000200040008080ULL,
    0x0000100020008080ULL, 0x0000080010008080ULL, 0x0000040008008080ULL,
    0x0000020004008080ULL, 0x0000010002008080ULL, 0x0000004081020004ULL,
    0x0000204000800080ULL, 0x0000200040008080ULL, 0x0000100020008080ULL,
    0x0000080010008080ULL, 0x0000040008008080ULL, 0x0000020004008080ULL,
    0x0000800100020080ULL, 0x0000800041000080ULL, 0x00FFFCDDFCED714AULL,
    0x007FFCDDFCED714AULL, 0x003FFFCDFFD88096ULL, 0x0000040810002101ULL,
    0x0001000204080011ULL, 0x0001000204000801ULL, 0x0001000082000401ULL,
    0x0001FFFAABFAD1A2ULL};

// Fixed magic numbers for bishops
constexpr Bitboard BishopMagicNumbers[64] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL,
    0x0004040080000000ULL, 0x0001104000000000ULL, 0x0000821040000000ULL,
    0x0000410410400000ULL, 0x0000104104104000ULL, 0x0000040404040400ULL,
    0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
    0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL,
    0x0000002082082000ULL, 0x0004000808080800ULL, 0x0002000404040400ULL,
    0x0001000202020200ULL, 0x0000800802004000ULL, 0x0000800400A00000ULL,
    0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
    0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL,
    0x0000404004010200ULL, 0x0000840000802000ULL, 0x0000404002011000ULL,
    0x0000808001041000ULL, 0x0000404000820800ULL, 0x0001041000202000ULL,
    0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL,
    0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL,
    0x0000808080010400ULL, 0x0000820820004000ULL, 0x0000410410002000ULL,
    0x0000082088001000ULL, 0x0000002011000800ULL, 0x0000080100400400ULL,
    0x0001010101000200ULL, 0x0002020202000400ULL, 0x0001010101000200ULL,
    0x0000410410400000ULL, 0x0000208208200000ULL, 0x0000002084100000ULL,
    0x0000000020880000ULL, 0x0000001002020000ULL, 0x0000040408020000ULL,
    0x0004040404040000ULL, 0x0002020202020000ULL, 0x0000104104104000ULL,
    0x0000002082082000ULL, 0x0000000020841000ULL, 0x0000000000208800ULL,
    0x0000000010020200ULL, 0x0000000404080200ULL, 0x0000040404040400ULL,
    0x0002020202020200ULL};

void init() {
  // Initialize rook magics
  int rookTableIndex = 0;
  for (Square sq = A1; sq <= H8; ++sq) {
    RookMagics[sq].mask = relevantOccupancy(sq, RookDirections);
    RookMagics[sq].magic = RookMagicNumbers[sq];
    RookMagics[sq].attacks = &RookTable[rookTableIndex];
    RookMagics[sq].shift = 64 - BB::popCount(RookMagics[sq].mask);

    // Fill attack table for all possible occupancies
    Bitboard mask = RookMagics[sq].mask;
    int n = BB::popCount(mask);
    for (int i = 0; i < (1 << n); ++i) {
      // Generate occupancy variation
      Bitboard occupied = BB::EMPTY;
      Bitboard tempMask = mask;
      for (int j = 0; j < n; ++j) {
        Square bit = BB::popLsb(tempMask);
        if (i & (1 << j)) {
          occupied |= BB::squareBB(bit);
        }
      }

      int index = magicIndex(RookMagics[sq], occupied);
      RookMagics[sq].attacks[index] =
          slidingAttacks(sq, occupied, RookDirections);
    }

    rookTableIndex += (1 << n);
  }

  // Initialize bishop magics
  int bishopTableIndex = 0;
  for (Square sq = A1; sq <= H8; ++sq) {
    BishopMagics[sq].mask = relevantOccupancy(sq, BishopDirections);
    BishopMagics[sq].magic = BishopMagicNumbers[sq];
    BishopMagics[sq].attacks = &BishopTable[bishopTableIndex];
    BishopMagics[sq].shift = 64 - BB::popCount(BishopMagics[sq].mask);

    // Fill attack table
    Bitboard mask = BishopMagics[sq].mask;
    int n = BB::popCount(mask);
    for (int i = 0; i < (1 << n); ++i) {
      Bitboard occupied = BB::EMPTY;
      Bitboard tempMask = mask;
      for (int j = 0; j < n; ++j) {
        Square bit = BB::popLsb(tempMask);
        if (i & (1 << j)) {
          occupied |= BB::squareBB(bit);
        }
      }

      int index = magicIndex(BishopMagics[sq], occupied);
      BishopMagics[sq].attacks[index] =
          slidingAttacks(sq, occupied, BishopDirections);
    }

    bishopTableIndex += (1 << n);
  }

  // Initialize between and line tables
  for (Square sq1 = A1; sq1 <= H8; ++sq1) {
    for (Square sq2 = A1; sq2 <= H8; ++sq2) {
      Bitboard sq1BB = BB::squareBB(sq1);
      Bitboard sq2BB = BB::squareBB(sq2);

      // Check if squares are on same rank, file, or diagonal
      Bitboard rookAttack = rookAttacks(sq1, BB::EMPTY);
      Bitboard bishopAttack = bishopAttacks(sq1, BB::EMPTY);

      if (rookAttack & sq2BB) {
        LineTable[sq1][sq2] =
            (rookAttacks(sq1, BB::EMPTY) & rookAttacks(sq2, BB::EMPTY)) |
            sq1BB | sq2BB;
        BetweenTable[sq1][sq2] =
            rookAttacks(sq1, sq2BB) & rookAttacks(sq2, sq1BB);
      } else if (bishopAttack & sq2BB) {
        LineTable[sq1][sq2] =
            (bishopAttacks(sq1, BB::EMPTY) & bishopAttacks(sq2, BB::EMPTY)) |
            sq1BB | sq2BB;
        BetweenTable[sq1][sq2] =
            bishopAttacks(sq1, sq2BB) & bishopAttacks(sq2, sq1BB);
      } else {
        LineTable[sq1][sq2] = BB::EMPTY;
        BetweenTable[sq1][sq2] = BB::EMPTY;
      }
    }
  }
}

Bitboard rookAttacks(Square sq, Bitboard occupied) {
  return RookMagics[sq].attacks[magicIndex(RookMagics[sq], occupied)];
}

Bitboard bishopAttacks(Square sq, Bitboard occupied) {
  return BishopMagics[sq].attacks[magicIndex(BishopMagics[sq], occupied)];
}

Bitboard between(Square sq1, Square sq2) { return BetweenTable[sq1][sq2]; }

Bitboard line(Square sq1, Square sq2) { return LineTable[sq1][sq2]; }

bool aligned(Square sq1, Square sq2, Square sq3) {
  return LineTable[sq1][sq2] & BB::squareBB(sq3);
}

}  // namespace Magic
