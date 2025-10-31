#include "Bitboard.h"

#include <iomanip>
#include <sstream>

namespace BB {

// Attack tables
Bitboard PawnAttacks[2][64];
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard BetweenBB[64][64];

// Knight move offsets
constexpr int KnightOffsets[8] = {-17, -15, -10, -6, 6, 10, 15, 17};

// King move offsets
constexpr int KingOffsets[8] = {-9, -8, -7, -1, 1, 7, 8, 9};

// Check if a square is valid
bool isValid(int sq) { return sq >= 0 && sq < 64; }

// Check if knight move is valid (no wrapping)
bool isValidKnightMove(Square from, Square to) {
  int fileDiff = std::abs(fileOf(from) - fileOf(to));
  int rankDiff = std::abs(rankOf(from) - rankOf(to));
  return (fileDiff == 1 && rankDiff == 2) || (fileDiff == 2 && rankDiff == 1);
}

// Check if king move is valid (no wrapping)
bool isValidKingMove(Square from, Square to) {
  int fileDiff = std::abs(fileOf(from) - fileOf(to));
  int rankDiff = std::abs(rankOf(from) - rankOf(to));
  return fileDiff <= 1 && rankDiff <= 1;
}

void init() {
  // Initialize pawn attacks
  for (Square sq = A1; sq <= H8; ++sq) {
    // White pawns
    Bitboard bb = squareBB(sq);
    PawnAttacks[WHITE][sq] =
        pawnAttackWest<WHITE>(bb) | pawnAttackEast<WHITE>(bb);

    // Black pawns
    PawnAttacks[BLACK][sq] =
        pawnAttackWest<BLACK>(bb) | pawnAttackEast<BLACK>(bb);
  }

  // Initialize knight attacks
  for (Square sq = A1; sq <= H8; ++sq) {
    Bitboard attacks = EMPTY;
    for (int offset : KnightOffsets) {
      int to = sq + offset;
      if (isValid(to) && isValidKnightMove(sq, Square(to))) {
        attacks |= squareBB(Square(to));
      }
    }
    KnightAttacks[sq] = attacks;
  }

  // Initialize king attacks
  for (Square sq = A1; sq <= H8; ++sq) {
    Bitboard attacks = EMPTY;
    for (int offset : KingOffsets) {
      int to = sq + offset;
      if (isValid(to) && isValidKingMove(sq, Square(to))) {
        attacks |= squareBB(Square(to));
      }
    }
    KingAttacks[sq] = attacks;
  }

  // Initialize between bitboards (for pin detection)
  for (Square sq1 = A1; sq1 <= H8; ++sq1) {
    for (Square sq2 = A1; sq2 <= H8; ++sq2) {
      Bitboard between = EMPTY;

      if (sq1 == sq2) {
        BetweenBB[sq1][sq2] = EMPTY;
        continue;
      }

      int file1 = fileOf(sq1), rank1 = rankOf(sq1);
      int file2 = fileOf(sq2), rank2 = rankOf(sq2);
      int fileDiff = file2 - file1;
      int rankDiff = rank2 - rank1;

      // Only for aligned squares (same rank, file, or diagonal)
      if (fileDiff != 0 && rankDiff != 0 &&
          std::abs(fileDiff) != std::abs(rankDiff)) {
        BetweenBB[sq1][sq2] = EMPTY;
        continue;
      }

      int fileStep = (fileDiff == 0) ? 0 : (fileDiff > 0 ? 1 : -1);
      int rankStep = (rankDiff == 0) ? 0 : (rankDiff > 0 ? 1 : -1);

      int file = file1 + fileStep;
      int rank = rank1 + rankStep;

      while (file != file2 || rank != rank2) {
        between |= squareBB(makeSquare(file, rank));
        file += fileStep;
        rank += rankStep;
      }

      BetweenBB[sq1][sq2] = between;
    }
  }
}

std::string toString(Bitboard bb) {
  std::ostringstream oss;
  oss << "\n";
  for (int rank = 7; rank >= 0; --rank) {
    oss << (rank + 1) << " ";
    for (int file = 0; file < 8; ++file) {
      Square sq = makeSquare(file, rank);
      oss << (testBit(bb, sq) ? "1 " : ". ");
    }
    oss << "\n";
  }
  oss << "  a b c d e f g h\n";
  oss << "  Bitboard: 0x" << std::hex << std::setw(16) << std::setfill('0')
      << bb << std::dec;
  return oss.str();
}

}  // namespace BB
