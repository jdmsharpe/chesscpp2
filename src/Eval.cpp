#include "Eval.h"

#include "Bitboard.h"
#include "Magic.h"
#include "Types.h"
#include "Zobrist.h"

namespace {
// Piece-square tables for positional evaluation

// Pawns - encourage center control and advancement
constexpr int pawn[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,    // Rank 1
    5,   10,  10, -20, -20,  10,  10,  5,    // Rank 2
    5,   10,  20,  40,  40,  20,  10,  5,    // Rank 3
    10,  15,  30,  70,  70,  30,  15,  10,   // Rank 4
    15,  20,  35,  80,  80,  35,  20,  15,   // Rank 5
    20,  25,  30,  35,  35,  30,  25,  20,   // Rank 6
    50,  50,  50,  50,  50,  50,  50,  50,   // Rank 7
    0,   0,   0,   0,   0,   0,   0,   0};   // Rank 8

// Knights - heavily prefer center, punish edges
constexpr int knight[64] = {
    -50, -40, -30, -25, -25, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -25,   5,  15,  20,  20,  15,   5, -25,
    -25,   5,  15,  20,  20,  15,   5, -25,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -25, -25, -30, -40, -50};

// Bishops - prefer long diagonals and center
constexpr int bishop[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  15,  15,  10,   0, -10,
    -10,   5,   5,  15,  15,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

// Rooks - prefer 7th rank and open files
constexpr int rook[64] = {
     0,   0,   0,   5,   5,   0,   0,   0,
    20,  20,  20,  20,  20,  20,  20,  20,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
     0,   0,   0,   0,   0,   0,   0,   0};

// Kings - stay safe in middlegame
constexpr int kingMiddle[64] = {
     20,  30,  10,   0,   0,  10,  30,  20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30};

// Kings - centralize in endgame
constexpr int kingEndgame[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   5,  10,  15,  15,  10,   5, -10,
    -10,   5,  15,  20,  20,  15,   5, -10,
    -10,   5,  15,  20,  20,  15,   5, -10,
    -10,   5,  10,  15,  15,  10,   5, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

// ============================================================================
// [Improvement 5] Pawn Hash Table
// ============================================================================
struct PawnEntry {
  HashKey pawnKey = 0;
  int score = 0;  // White - Black pawn structure score
};

static constexpr size_t PAWN_HASH_SIZE = 16384;  // 16K entries
static PawnEntry pawnHashTable[PAWN_HASH_SIZE];

static HashKey computePawnKey(const Position& pos) {
  HashKey key = 0;
  Bitboard wp = pos.pieces(WHITE, PAWN);
  while (wp) {
    Square sq = BB::popLsb(wp);
    key ^= Zobrist::psq[W_PAWN][sq];
  }
  Bitboard bp = pos.pieces(BLACK, PAWN);
  while (bp) {
    Square sq = BB::popLsb(bp);
    key ^= Zobrist::psq[B_PAWN][sq];
  }
  return key;
}

// ============================================================================
// Pawn structure evaluation (cached via pawn hash)
// ============================================================================
static int evaluatePawnStructureSide(const Position& pos, Color c) {
  int score = 0;
  Bitboard pawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (pawns) {
    Square sq = BB::popLsb(pawns);
    int file = fileOf(sq);
    int rank = rankOf(sq);

    // Doubled pawns penalty
    Bitboard filePawns = pos.pieces(c, PAWN) & BB::fileBB(file);
    if (BB::popCount(filePawns) > 1) {
      score -= 10;
    }

    // Isolated pawns penalty
    Bitboard adjacentFiles = 0ULL;
    if (file > 0) adjacentFiles |= BB::fileBB(file - 1);
    if (file < 7) adjacentFiles |= BB::fileBB(file + 1);

    if (!(pos.pieces(c, PAWN) & adjacentFiles)) {
      score -= 15;
    }

    // Passed pawns bonus
    Bitboard passedMask = 0ULL;
    if (c == WHITE) {
      for (int r = rank + 1; r < 8; ++r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    } else {
      for (int r = rank - 1; r >= 0; --r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    }

    if (!(enemyPawns & passedMask)) {
      int advancement = c == WHITE ? rank - 1 : 6 - rank;
      static constexpr int advancementBonus[6] = {10, 15, 25, 40, 70, 120};
      int bonus = advancementBonus[advancement];

      // Clear path bonus
      Bitboard pathAhead = 0ULL;
      if (c == WHITE) {
        for (int r = rank + 1; r < 8; ++r)
          pathAhead |= BB::squareBB(r * 8 + file);
      } else {
        for (int r = rank - 1; r >= 0; --r)
          pathAhead |= BB::squareBB(r * 8 + file);
      }
      if (!(pos.occupied() & pathAhead)) {
        bonus += 15;
      }

      score += 20 + bonus;
    } else {
      // Backward pawn detection (fixed from original bugged version)
      // A pawn is backward if no friendly pawn on adjacent files can support it
      // AND the stop square is controlled by an enemy pawn
      Bitboard adjacentFriendly = pos.pieces(c, PAWN) & adjacentFiles;
      if (adjacentFriendly) {
        // Check if all adjacent friendly pawns are ahead (can't support)
        bool allAhead = true;
        Bitboard adj = adjacentFriendly;
        while (adj) {
          Square adjSq = BB::popLsb(adj);
          int adjRank = rankOf(adjSq);
          if (c == WHITE) {
            if (adjRank <= rank) allAhead = false;
          } else {
            if (adjRank >= rank) allAhead = false;
          }
        }
        if (allAhead) {
          // Check if stop square is controlled by enemy pawn
          Square stopSq = (c == WHITE) ? Square(sq + 8) : Square(sq - 8);
          if (stopSq >= 0 && stopSq < 64) {
            Bitboard stopAttacks = BB::pawnAttacks(c, stopSq);
            if (enemyPawns & stopAttacks) {
              score -= 12;  // Backward pawn
            }
          }
        }
      }
    }

    // Pawn chain bonus
    Bitboard defenderMask = BB::pawnAttacks(~c, sq);
    if (defenderMask & pos.pieces(c, PAWN)) {
      score += 5;
    }
  }

  return score;
}

static int evaluatePawnStructure(const Position& pos) {
  HashKey pawnKey = computePawnKey(pos);
  size_t idx = pawnKey % PAWN_HASH_SIZE;
  PawnEntry& entry = pawnHashTable[idx];

  if (entry.pawnKey == pawnKey) {
    return entry.score;  // Cache hit
  }

  // Cache miss — compute and store
  int score = evaluatePawnStructureSide(pos, WHITE) -
              evaluatePawnStructureSide(pos, BLACK);
  entry.pawnKey = pawnKey;
  entry.score = score;
  return score;
}

// ============================================================================
// [Improvement 8] King Safety with Attack Unit Counting
// ============================================================================
static int evaluateKingSafety(const Position& pos, Color c) {
  int score = 0;
  Square kingSq = BB::lsb(pos.pieces(c, KING));
  int kingFile = fileOf(kingSq);

  // Pawn shield bonus
  Bitboard pawns = pos.pieces(c, PAWN);
  if (c == WHITE) {
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(1 * 8 + f)) score += 10;
      if (pawns & BB::squareBB(2 * 8 + f)) score += 5;
    }
  } else {
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(6 * 8 + f)) score += 10;
      if (pawns & BB::squareBB(5 * 8 + f)) score += 5;
    }
  }

  // Open file near king penalty
  for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
       ++f) {
    Bitboard filePawns =
        (pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN)) & BB::fileBB(f);
    if (!filePawns) {
      score -= 20;
    }
  }

  // --- Attack unit counting ---
  Color them = ~c;
  Bitboard kingZone = BB::kingAttacks(kingSq) | BB::squareBB(kingSq);
  Bitboard occupied = pos.occupied();
  int attackUnits = 0;
  int numAttackers = 0;

  // Enemy knights attacking king zone
  Bitboard enemyKnights = pos.pieces(them, KNIGHT);
  while (enemyKnights) {
    Square sq = BB::popLsb(enemyKnights);
    if (BB::knightAttacks(sq) & kingZone) {
      attackUnits += 2;
      numAttackers++;
    }
  }

  // Enemy bishops attacking king zone
  Bitboard enemyBishops = pos.pieces(them, BISHOP);
  while (enemyBishops) {
    Square sq = BB::popLsb(enemyBishops);
    if (Magic::bishopAttacks(sq, occupied) & kingZone) {
      attackUnits += 2;
      numAttackers++;
    }
  }

  // Enemy rooks attacking king zone
  Bitboard enemyRooks = pos.pieces(them, ROOK);
  while (enemyRooks) {
    Square sq = BB::popLsb(enemyRooks);
    if (Magic::rookAttacks(sq, occupied) & kingZone) {
      attackUnits += 3;
      numAttackers++;
    }
  }

  // Enemy queens attacking king zone
  Bitboard enemyQueens = pos.pieces(them, QUEEN);
  while (enemyQueens) {
    Square sq = BB::popLsb(enemyQueens);
    if (Magic::queenAttacks(sq, occupied) & kingZone) {
      attackUnits += 5;
      numAttackers++;
    }
  }

  // Non-linear penalty: attacks become exponentially more dangerous
  // Only apply when at least 2 pieces are attacking (1 attacker is normal)
  if (numAttackers >= 2) {
    score -= attackUnits * attackUnits / 2;
  }

  return score;
}

static int evaluateMobility(const Position& pos, Color c) {
  int mobility = 0;

  Bitboard knights = pos.pieces(c, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    Bitboard attacks = BB::knightAttacks(sq) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  Bitboard bishops = pos.pieces(c, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    Bitboard attacks =
        Magic::bishopAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  Bitboard rooks = pos.pieces(c, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    Bitboard attacks = Magic::rookAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  Bitboard queens = pos.pieces(c, QUEEN);
  while (queens) {
    Square sq = BB::popLsb(queens);
    Bitboard attacks = (Magic::bishopAttacks(sq, pos.occupied()) |
                        Magic::rookAttacks(sq, pos.occupied())) &
                       ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  return mobility * 2;
}

static int getGamePhase(const Position& pos) {
  int phase = 0;
  phase += BB::popCount(pos.pieces(WHITE, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(WHITE, BISHOP)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, BISHOP)) * 1;
  phase += BB::popCount(pos.pieces(WHITE, ROOK)) * 2;
  phase += BB::popCount(pos.pieces(BLACK, ROOK)) * 2;
  phase += BB::popCount(pos.pieces(WHITE, QUEEN)) * 4;
  phase += BB::popCount(pos.pieces(BLACK, QUEEN)) * 4;
  const int TOTAL_PHASE = 24;
  return std::min(256, (phase * 256 + TOTAL_PHASE / 2) / TOTAL_PHASE);
}

static int evaluateDevelopment(const Position& pos, Color c) {
  int score = 0;

  if (c == WHITE) {
    if (pos.pieceAt(B1) == makePiece(WHITE, KNIGHT)) score -= 20;
    if (pos.pieceAt(G1) == makePiece(WHITE, KNIGHT)) score -= 20;
    if (pos.pieceAt(C1) == makePiece(WHITE, BISHOP)) score -= 15;
    if (pos.pieceAt(F1) == makePiece(WHITE, BISHOP)) score -= 15;
    if (pos.pieceAt(A1) == makePiece(WHITE, ROOK)) score -= 5;
    if (pos.pieceAt(H1) == makePiece(WHITE, ROOK)) score -= 5;

    Square queenSq = BB::lsb(pos.pieces(WHITE, QUEEN));
    if (queenSq != D1 && queenSq != NO_SQUARE) {
      int minorsDeveloped = 0;
      if (pos.pieceAt(B1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;
      if (minorsDeveloped < 2) score -= 30;
    }

    Square kingSq = BB::lsb(pos.pieces(WHITE, KING));
    if (kingSq == G1 || kingSq == C1) score += 40;
    if (pos.pieceAt(E4) == makePiece(WHITE, PAWN)) score += 50;
    if (pos.pieceAt(D4) == makePiece(WHITE, PAWN)) score += 50;

  } else {
    if (pos.pieceAt(B8) == makePiece(BLACK, KNIGHT)) score -= 20;
    if (pos.pieceAt(G8) == makePiece(BLACK, KNIGHT)) score -= 20;
    if (pos.pieceAt(C8) == makePiece(BLACK, BISHOP)) score -= 15;
    if (pos.pieceAt(F8) == makePiece(BLACK, BISHOP)) score -= 15;
    if (pos.pieceAt(A8) == makePiece(BLACK, ROOK)) score -= 5;
    if (pos.pieceAt(H8) == makePiece(BLACK, ROOK)) score -= 5;

    Square queenSq = BB::lsb(pos.pieces(BLACK, QUEEN));
    if (queenSq != D8 && queenSq != NO_SQUARE) {
      int minorsDeveloped = 0;
      if (pos.pieceAt(B8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;
      if (minorsDeveloped < 2) score -= 30;
    }

    Square kingSq = BB::lsb(pos.pieces(BLACK, KING));
    if (kingSq == G8 || kingSq == C8) score += 40;
    if (pos.pieceAt(E5) == makePiece(BLACK, PAWN)) score += 50;
    if (pos.pieceAt(D5) == makePiece(BLACK, PAWN)) score += 50;
  }

  return score;
}

static int evaluateRooks(const Position& pos, Color c) {
  int score = 0;
  Bitboard rooks = pos.pieces(c, ROOK);
  Bitboard ourPawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (rooks) {
    Square sq = BB::popLsb(rooks);
    int file = fileOf(sq);
    Bitboard fileMask = BB::fileBB(file);

    bool hasOurPawns = (ourPawns & fileMask) != 0;
    bool hasEnemyPawns = (enemyPawns & fileMask) != 0;

    if (!hasOurPawns && !hasEnemyPawns) {
      score += 25;
    } else if (!hasOurPawns && hasEnemyPawns) {
      score += 15;
    }

    int rank = rankOf(sq);
    if ((c == WHITE && rank == RANK_7) || (c == BLACK && rank == RANK_2)) {
      score += 20;
    }

    // Rook behind passed pawn
    Bitboard allPawnsOnFile = (ourPawns | enemyPawns) & fileMask;
    Bitboard pawnIter = allPawnsOnFile;
    while (pawnIter) {
      Square pawnSq = BB::popLsb(pawnIter);
      int pawnRank = rankOf(pawnSq);
      Color pawnColor = (ourPawns & BB::squareBB(pawnSq)) ? c : ~c;

      Bitboard pMask = 0ULL;
      Bitboard pEnemy = pos.pieces(~pawnColor, PAWN);
      int pFile = fileOf(pawnSq);
      if (pawnColor == WHITE) {
        for (int r = pawnRank + 1; r < 8; ++r) {
          pMask |= BB::squareBB(r * 8 + pFile);
          if (pFile > 0) pMask |= BB::squareBB(r * 8 + pFile - 1);
          if (pFile < 7) pMask |= BB::squareBB(r * 8 + pFile + 1);
        }
      } else {
        for (int r = pawnRank - 1; r >= 0; --r) {
          pMask |= BB::squareBB(r * 8 + pFile);
          if (pFile > 0) pMask |= BB::squareBB(r * 8 + pFile - 1);
          if (pFile < 7) pMask |= BB::squareBB(r * 8 + pFile + 1);
        }
      }

      if (!(pEnemy & pMask)) {
        bool rookBehind = false;
        if (pawnColor == WHITE)
          rookBehind = rank < pawnRank;
        else
          rookBehind = rank > pawnRank;

        if (rookBehind) {
          if (pawnColor == c)
            score += 20;
          else
            score += 15;
        }
      }
    }
  }

  return score;
}

static int evaluateBishops(const Position& pos, Color c) {
  int score = 0;
  Bitboard bishops = pos.pieces(c, BISHOP);
  if (BB::popCount(bishops) >= 2) {
    score += 30;
  }
  return score;
}

static int evaluateKnights(const Position& pos, Color c) {
  int score = 0;
  Bitboard knights = pos.pieces(c, KNIGHT);
  Bitboard ourPawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);

  while (knights) {
    Square sq = BB::popLsb(knights);
    int file = fileOf(sq);
    int rank = rankOf(sq);

    bool isOutpostRank = false;
    if (c == WHITE && (rank == RANK_4 || rank == RANK_5 || rank == RANK_6))
      isOutpostRank = true;
    else if (c == BLACK &&
             (rank == RANK_5 || rank == RANK_4 || rank == RANK_3))
      isOutpostRank = true;

    if (isOutpostRank) {
      Bitboard defenderMask = BB::pawnAttacks(~c, sq);
      bool defendedByPawn = (defenderMask & ourPawns) != 0;

      if (defendedByPawn) {
        bool canBeAttacked = false;
        if (c == WHITE) {
          for (int r = rank; r < 8; ++r) {
            if (file > 0 &&
                (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 &&
                (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        } else {
          for (int r = rank; r >= 0; --r) {
            if (file > 0 &&
                (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 &&
                (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        }

        if (!canBeAttacked) {
          int outpostBonus = 25;
          if (file >= 2 && file <= 5) outpostBonus += 10;
          score += outpostBonus;
        }
      }
    }
  }

  return score;
}

static int centerDistance(Square sq) {
  int file = fileOf(sq);
  int rank = rankOf(sq);
  int fileDist = std::max(3 - file, file - 4);
  int rankDist = std::max(3 - rank, rank - 4);
  return fileDist + rankDist;
}

static int kingDistance(Square a, Square b) {
  return std::max(std::abs(fileOf(a) - fileOf(b)),
                  std::abs(rankOf(a) - rankOf(b)));
}

static int evaluateMopUp(const Position& pos, int materialBalance, int phase) {
  if (phase > 128 || std::abs(materialBalance) < 400) return 0;

  Color winner = materialBalance > 0 ? WHITE : BLACK;
  Square winnerKing = BB::lsb(pos.pieces(winner, KING));
  Square loserKing = BB::lsb(pos.pieces(~winner, KING));

  int bonus = centerDistance(loserKing) * 10 +
              (7 - kingDistance(winnerKing, loserKing)) * 5;

  return winner == WHITE ? bonus : -bonus;
}

static int evaluateKingPawnProximity(const Position& pos, Color c) {
  int score = 0;
  Bitboard pawns = pos.pieces(c, PAWN);
  Bitboard enemyPawns = pos.pieces(~c, PAWN);
  Square ourKing = BB::lsb(pos.pieces(c, KING));
  Square theirKing = BB::lsb(pos.pieces(~c, KING));

  Bitboard pawnIter = pawns;
  while (pawnIter) {
    Square sq = BB::popLsb(pawnIter);
    int file = fileOf(sq);
    int rank = rankOf(sq);

    Bitboard passedMask = 0ULL;
    if (c == WHITE) {
      for (int r = rank + 1; r < 8; ++r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    } else {
      for (int r = rank - 1; r >= 0; --r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    }

    if (!(enemyPawns & passedMask)) {
      int ourDist = kingDistance(ourKing, sq);
      int theirDist = kingDistance(theirKing, sq);
      score += (7 - ourDist) * 5;
      score += theirDist * 5;

      int promoRank = (c == WHITE) ? 7 : 0;
      int distToPromo = std::abs(promoRank - rank);

      Bitboard enemyPieces = pos.pieces(~c, KNIGHT) | pos.pieces(~c, BISHOP) |
                             pos.pieces(~c, ROOK) | pos.pieces(~c, QUEEN);

      if (!enemyPieces && distToPromo > 0) {
        Square promoSq = Square(promoRank * 8 + file);
        int kingDistToPromo = kingDistance(theirKing, promoSq);
        int pawnDist = distToPromo;
        if ((c == WHITE && rank == RANK_2) || (c == BLACK && rank == RANK_7)) {
          pawnDist--;
        }

        if (kingDistToPromo > pawnDist + 1) {
          score += 700;
        } else if (kingDistToPromo > pawnDist) {
          score += 350;
        }
      }
    }
  }

  return score;
}

}  // anonymous namespace

namespace Eval {
int evaluate(const Position& pos) {
  int material = pos.materialCount(WHITE) - pos.materialCount(BLACK);

  int positional = 0;

  // White pieces
  Bitboard pawns = pos.pieces(WHITE, PAWN);
  while (pawns) {
    Square sq = BB::popLsb(pawns);
    positional += pawn[sq];
  }
  Bitboard knights = pos.pieces(WHITE, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    positional += knight[sq];
  }
  Bitboard bishops = pos.pieces(WHITE, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    positional += bishop[sq];
  }
  Bitboard rooks = pos.pieces(WHITE, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    positional += rook[sq];
  }

  int kingPositionalMG = 0;
  int kingPositionalEG = 0;
  Bitboard kings = pos.pieces(WHITE, KING);
  while (kings) {
    Square sq = BB::popLsb(kings);
    kingPositionalMG += kingMiddle[sq];
    kingPositionalEG += kingEndgame[sq];
  }

  // Black pieces (flip board)
  pawns = pos.pieces(BLACK, PAWN);
  while (pawns) {
    Square sq = BB::popLsb(pawns);
    positional -= pawn[sq ^ 56];
  }
  knights = pos.pieces(BLACK, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    positional -= knight[sq ^ 56];
  }
  bishops = pos.pieces(BLACK, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    positional -= bishop[sq ^ 56];
  }
  rooks = pos.pieces(BLACK, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    positional -= rook[sq ^ 56];
  }
  kings = pos.pieces(BLACK, KING);
  while (kings) {
    Square sq = BB::popLsb(kings);
    kingPositionalMG -= kingMiddle[sq ^ 56];
    kingPositionalEG -= kingEndgame[sq ^ 56];
  }

  // [Improvement 5] Pawn structure via hash table
  int pawnStructure = evaluatePawnStructure(pos);

  int kingSafety =
      evaluateKingSafety(pos, WHITE) - evaluateKingSafety(pos, BLACK);
  int mobility = evaluateMobility(pos, WHITE) - evaluateMobility(pos, BLACK);
  int development =
      evaluateDevelopment(pos, WHITE) - evaluateDevelopment(pos, BLACK);
  int rookScore = evaluateRooks(pos, WHITE) - evaluateRooks(pos, BLACK);
  int bishopScore = evaluateBishops(pos, WHITE) - evaluateBishops(pos, BLACK);
  int knightScore = evaluateKnights(pos, WHITE) - evaluateKnights(pos, BLACK);
  int kingPawnProx = evaluateKingPawnProximity(pos, WHITE) -
                     evaluateKingPawnProximity(pos, BLACK);

  int phase = getGamePhase(pos);

  int openingScore = material + positional + kingPositionalMG + mobility +
                     kingSafety + pawnStructure + development + rookScore +
                     bishopScore + knightScore;

  int endgameScore = material + (positional / 2) + kingPositionalEG +
                     (mobility / 2) + (kingSafety / 4) +
                     (pawnStructure * 3 / 2) + (rookScore * 3 / 2) +
                     bishopScore + knightScore + kingPawnProx;

  int score = (openingScore * phase + endgameScore * (256 - phase)) / 256;

  score += evaluateMopUp(pos, material, phase);

  // 50-move rule scaling
  int hmc = pos.halfmoveClock();
  if (hmc > 70 && score != 0) {
    int scaleFactor;
    if (hmc < 90) {
      scaleFactor = 256 - (hmc - 70) * (256 - 128) / 20;
    } else {
      scaleFactor = 128 - (hmc - 90) * (128 - 13) / 10;
    }
    scaleFactor = std::max(scaleFactor, 13);
    score = score * scaleFactor / 256;
  }

  return (pos.sideToMove() == WHITE) ? score : -score;
}
}  // namespace Eval
