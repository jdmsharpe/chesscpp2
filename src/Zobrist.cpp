#include "Zobrist.h"

#include <random>

namespace Zobrist {

HashKey psq[12][64];
HashKey enpassant[8];
HashKey castling[16];
HashKey sideToMove;

void init() {
  // Use a fixed seed for reproducibility
  std::mt19937_64 rng(0x123456789ABCDEF0ULL);
  std::uniform_int_distribution<HashKey> dist;

  // Piece-square keys
  for (int pc = 0; pc < 12; ++pc) {
    for (int sq = 0; sq < 64; ++sq) {
      psq[pc][sq] = dist(rng);
    }
  }

  // En passant file keys
  for (int file = 0; file < 8; ++file) {
    enpassant[file] = dist(rng);
  }

  // Castling rights keys
  for (int cr = 0; cr < 16; ++cr) {
    castling[cr] = dist(rng);
  }

  // Side to move key
  sideToMove = dist(rng);
}

}  // namespace Zobrist
