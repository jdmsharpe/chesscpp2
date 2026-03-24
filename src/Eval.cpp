#include "Eval.h"

#include "Bitboard.h"
#include "Magic.h"
#include "Types.h"

namespace {
// Piece-square tables for positional evaluation (constexpr for compile-time
// init)

// Pawns - STRONGLY encourage center control and advancement
constexpr int pawn[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,    // Rank 1
    5,   10,  10, -20, -20,  10,  10,  5,    // Rank 2 (discourage early edge pawns)
    5,   10,  20,  40,  40,  20,  10,  5,    // Rank 3 (increased center)
    10,  15,  30,  70,  70,  30,  15,  10,   // Rank 4 (MAJOR bonus for e4/d4!)
    15,  20,  35,  80,  80,  35,  20,  15,   // Rank 5 (advanced center - huge bonus)
    20,  25,  30,  35,  35,  30,  25,  20,   // Rank 6
    50,  50,  50,  50,  50,  50,  50,  50,   // Rank 7 (about to promote)
    0,   0,   0,   0,   0,   0,   0,   0};   // Rank 8

// Knights - heavily prefer center, punish edges
constexpr int knight[64] = {
    -50, -40, -30, -25, -25, -30, -40, -50,  // Rank 1 (bad)
    -40, -20,   0,   5,   5,   0, -20, -40,  // Rank 2
    -30,   5,  10,  15,  15,  10,   5, -30,  // Rank 3
    -25,   5,  15,  20,  20,  15,   5, -25,  // Rank 4
    -25,   5,  15,  20,  20,  15,   5, -25,  // Rank 5
    -30,   5,  10,  15,  15,  10,   5, -30,  // Rank 6
    -40, -20,   0,   5,   5,   0, -20, -40,  // Rank 7
    -50, -40, -30, -25, -25, -30, -40, -50}; // Rank 8 (very bad)

// Bishops - prefer long diagonals and center
constexpr int bishop[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,  // Rank 1
    -10,   5,   0,   0,   0,   0,   5, -10,  // Rank 2
    -10,  10,  10,  10,  10,  10,  10, -10,  // Rank 3
    -10,   0,  10,  15,  15,  10,   0, -10,  // Rank 4
    -10,   5,   5,  15,  15,   5,   5, -10,  // Rank 5
    -10,   0,   5,  10,  10,   5,   0, -10,  // Rank 6
    -10,   5,   0,   0,   0,   0,   5, -10,  // Rank 7
    -20, -10, -10, -10, -10, -10, -10, -20}; // Rank 8

// Rooks - prefer 7th rank and open files (back rank OK)
constexpr int rook[64] = {
     0,   0,   0,   5,   5,   0,   0,   0,   // Rank 1
    20,  20,  20,  20,  20,  20,  20,  20,   // Rank 2 (7th rank for black)
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 3
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 4
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 5
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 6
    -5,   0,   0,   0,   0,   0,   0,  -5,   // Rank 7
     0,   0,   0,   0,   0,   0,   0,   0};  // Rank 8

// Kings - stay safe in middlegame (castled position)
constexpr int kingMiddle[64] = {
     20,  30,  10,   0,   0,  10,  30,  20,  // Rank 1 (castle!)
    -10, -20, -20, -20, -20, -20, -20, -10,  // Rank 2 (don't advance)
    -20, -30, -30, -40, -40, -30, -30, -20,  // Rank 3
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 4
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 5
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 6
    -30, -40, -40, -50, -50, -40, -40, -30,  // Rank 7
    -30, -40, -40, -50, -50, -40, -40, -30}; // Rank 8

// Kings - centralize in endgame (active king is critical)
constexpr int kingEndgame[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,  // Rank 1 (back rank bad)
    -10,   0,   5,   5,   5,   5,   0, -10,  // Rank 2
    -10,   5,  10,  15,  15,  10,   5, -10,  // Rank 3
    -10,   5,  15,  20,  20,  15,   5, -10,  // Rank 4
    -10,   5,  15,  20,  20,  15,   5, -10,  // Rank 5
    -10,   5,  10,  15,  15,  10,   5, -10,  // Rank 6
    -10,   0,   5,   5,   5,   5,   0, -10,  // Rank 7
    -20, -10, -10, -10, -10, -10, -10, -20}; // Rank 8

// All helper functions as static free functions

static int evaluatePawnStructure(const Position& pos, Color c) {
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
      score -= 10;  // Penalty for doubled pawns
    }

    // Isolated pawns penalty
    Bitboard adjacentFiles = 0ULL;
    if (file > 0) adjacentFiles |= BB::fileBB(file - 1);
    if (file < 7) adjacentFiles |= BB::fileBB(file + 1);

    if (!(pos.pieces(c, PAWN) & adjacentFiles)) {
      score -= 15;  // Penalty for isolated pawns
    }

    // Passed pawns bonus
    Bitboard passedMask = 0ULL;
    if (c == WHITE) {
      // For white, check files ahead (higher ranks)
      for (int r = rank + 1; r < 8; ++r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    } else {
      // For black, check files ahead (lower ranks)
      for (int r = rank - 1; r >= 0; --r) {
        passedMask |= BB::squareBB(r * 8 + file);
        if (file > 0) passedMask |= BB::squareBB(r * 8 + file - 1);
        if (file < 7) passedMask |= BB::squareBB(r * 8 + file + 1);
      }
    }

    if (!(enemyPawns & passedMask)) {
      // Passed pawn bonus with exponential advancement scaling
      // Ranks 2-7 for white (1-6 for black): the closer to promotion, the
      // more dangerous. A pawn on rank 7 is worth far more than rank 4.
      int advancement = c == WHITE ? rank - 1 : 6 - rank;  // 0-5 scale
      // Exponential curve: [0]=10, [1]=15, [2]=25, [3]=40, [4]=70, [5]=120
      static constexpr int advancementBonus[6] = {10, 15, 25, 40, 70, 120};
      int bonus = advancementBonus[advancement];

      // Extra bonus if the path ahead is clear (no blockers on the file)
      Bitboard pathAhead = 0ULL;
      if (c == WHITE) {
        for (int r = rank + 1; r < 8; ++r)
          pathAhead |= BB::squareBB(r * 8 + file);
      } else {
        for (int r = rank - 1; r >= 0; --r)
          pathAhead |= BB::squareBB(r * 8 + file);
      }
      if (!(pos.occupied() & pathAhead)) {
        bonus += 15;  // Clear path — pawn can advance freely
      }

      score += 20 + bonus;
    } else {
      // Check for backward pawns (not passed and cannot be defended by pawns)
      // A pawn is backward if:
      // 1. No friendly pawns on adjacent files can support it (all behind)
      // 2. Enemy pawns control the square in front of it

      bool hasSupport = false;
      Bitboard supportMask = 0ULL;

      // Check for supporting pawns on adjacent files behind this pawn
      if (c == WHITE) {
        for (int r = 0; r <= rank; ++r) {
          if (file > 0) supportMask |= BB::squareBB(r * 8 + file - 1);
          if (file < 7) supportMask |= BB::squareBB(r * 8 + file + 1);
        }
      } else {
        for (int r = rank; r < 8; ++r) {
          if (file > 0) supportMask |= BB::squareBB(r * 8 + file - 1);
          if (file < 7) supportMask |= BB::squareBB(r * 8 + file + 1);
        }
      }

      if (pos.pieces(c, PAWN) & supportMask) {
        hasSupport = true;
      }

      // If no support and not passed, this is a backward pawn
      if (!hasSupport && !((pos.pieces(c, PAWN) & adjacentFiles) != 0)) {
        score -= 12;  // Backward pawn penalty
      }
    }

    // Pawn chain bonus: pawns defending each other diagonally
    Bitboard defenderMask = BB::pawnAttacks(~c, sq);
    if (defenderMask & pos.pieces(c, PAWN)) {
      score += 5;  // Small bonus for being part of a pawn chain
    }
  }

  return score;
}

static int evaluateKingSafety(const Position& pos, Color c) {
  int score = 0;
  Square kingSq = BB::lsb(pos.pieces(c, KING));
  int kingFile = fileOf(kingSq);

  // Pawn shield bonus (pawns in front of king)
  Bitboard pawns = pos.pieces(c, PAWN);
  if (c == WHITE) {
    // Check pawns on rank 2 and 3 near king
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(1 * 8 + f)) score += 10;  // Pawn on 2nd rank
      if (pawns & BB::squareBB(2 * 8 + f)) score += 5;   // Pawn on 3rd rank
    }
  } else {
    // Check pawns on rank 7 and 6 near king
    for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1);
         ++f) {
      if (pawns & BB::squareBB(6 * 8 + f)) score += 10;  // Pawn on 7th rank
      if (pawns & BB::squareBB(5 * 8 + f)) score += 5;   // Pawn on 6th rank
    }
  }

  // Open file near king penalty
  for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
    Bitboard filePawns =
        (pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN)) & BB::fileBB(f);
    if (!filePawns) {
      score -= 20;  // Open file near king is dangerous
    }
  }

  return score;
}

static int evaluateMobility(const Position& pos, Color c) {
  // Count pseudo-legal moves as mobility metric
  int mobility = 0;

  // Knights
  Bitboard knights = pos.pieces(c, KNIGHT);
  while (knights) {
    Square sq = BB::popLsb(knights);
    Bitboard attacks = BB::knightAttacks(sq) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Bishops
  Bitboard bishops = pos.pieces(c, BISHOP);
  while (bishops) {
    Square sq = BB::popLsb(bishops);
    Bitboard attacks =
        Magic::bishopAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Rooks
  Bitboard rooks = pos.pieces(c, ROOK);
  while (rooks) {
    Square sq = BB::popLsb(rooks);
    Bitboard attacks = Magic::rookAttacks(sq, pos.occupied()) & ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Queens
  Bitboard queens = pos.pieces(c, QUEEN);
  while (queens) {
    Square sq = BB::popLsb(queens);
    Bitboard attacks = (Magic::bishopAttacks(sq, pos.occupied()) |
                        Magic::rookAttacks(sq, pos.occupied())) &
                       ~pos.pieces(c);
    mobility += BB::popCount(attacks);
  }

  // Weight mobility (each move worth ~2 centipawns)
  return mobility * 2;
}

static int getGamePhase(const Position& pos) {
  // Calculate game phase based on material
  // Opening: 256, Endgame: 0
  int phase = 0;

  // Knights and bishops: 1 point each
  phase += BB::popCount(pos.pieces(WHITE, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, KNIGHT)) * 1;
  phase += BB::popCount(pos.pieces(WHITE, BISHOP)) * 1;
  phase += BB::popCount(pos.pieces(BLACK, BISHOP)) * 1;

  // Rooks: 2 points each
  phase += BB::popCount(pos.pieces(WHITE, ROOK)) * 2;
  phase += BB::popCount(pos.pieces(BLACK, ROOK)) * 2;

  // Queens: 4 points each
  phase += BB::popCount(pos.pieces(WHITE, QUEEN)) * 4;
  phase += BB::popCount(pos.pieces(BLACK, QUEEN)) * 4;

  // Starting phase is 24 (4 knights + 4 bishops + 4 rooks + 2 queens = 4+4+8+8)
  // Scale to 256 for opening, 0 for endgame
  const int TOTAL_PHASE = 24;
  return std::min(256, (phase * 256 + TOTAL_PHASE / 2) / TOTAL_PHASE);
}

static int evaluateDevelopment(const Position& pos, Color c) {
  int score = 0;

  // Penalty for pieces still on starting squares (only in opening/middlegame)
  if (c == WHITE) {
    // Knights on starting squares
    if (pos.pieceAt(B1) == makePiece(WHITE, KNIGHT)) score -= 20;
    if (pos.pieceAt(G1) == makePiece(WHITE, KNIGHT)) score -= 20;

    // Bishops on starting squares
    if (pos.pieceAt(C1) == makePiece(WHITE, BISHOP)) score -= 15;
    if (pos.pieceAt(F1) == makePiece(WHITE, BISHOP)) score -= 15;

    // Rooks on starting squares (less penalty)
    if (pos.pieceAt(A1) == makePiece(WHITE, ROOK)) score -= 5;
    if (pos.pieceAt(H1) == makePiece(WHITE, ROOK)) score -= 5;

    // Queen moved too early penalty
    Square queenSq = BB::lsb(pos.pieces(WHITE, QUEEN));
    if (queenSq != D1 && queenSq != NO_SQUARE) {
      // Queen is off starting square - check if minor pieces developed
      int minorsDeveloped = 0;
      if (pos.pieceAt(B1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G1) != makePiece(WHITE, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F1) != makePiece(WHITE, BISHOP)) minorsDeveloped++;

      // Penalty if queen moved before developing pieces
      if (minorsDeveloped < 2) score -= 30;
    }

    // Castling bonus
    Square kingSq = BB::lsb(pos.pieces(WHITE, KING));
    if (kingSq == G1 || kingSq == C1) {
      score += 40;  // Big bonus for castling
    }

    // Center pawn bonus (e4/d4) - INCREASED to encourage central play
    if (pos.pieceAt(E4) == makePiece(WHITE, PAWN)) score += 50;
    if (pos.pieceAt(D4) == makePiece(WHITE, PAWN)) score += 50;

  } else {  // BLACK
    // Knights on starting squares
    if (pos.pieceAt(B8) == makePiece(BLACK, KNIGHT)) score -= 20;
    if (pos.pieceAt(G8) == makePiece(BLACK, KNIGHT)) score -= 20;

    // Bishops on starting squares
    if (pos.pieceAt(C8) == makePiece(BLACK, BISHOP)) score -= 15;
    if (pos.pieceAt(F8) == makePiece(BLACK, BISHOP)) score -= 15;

    // Rooks on starting squares (less penalty)
    if (pos.pieceAt(A8) == makePiece(BLACK, ROOK)) score -= 5;
    if (pos.pieceAt(H8) == makePiece(BLACK, ROOK)) score -= 5;

    // Queen moved too early penalty
    Square queenSq = BB::lsb(pos.pieces(BLACK, QUEEN));
    if (queenSq != D8 && queenSq != NO_SQUARE) {
      int minorsDeveloped = 0;
      if (pos.pieceAt(B8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(G8) != makePiece(BLACK, KNIGHT)) minorsDeveloped++;
      if (pos.pieceAt(C8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;
      if (pos.pieceAt(F8) != makePiece(BLACK, BISHOP)) minorsDeveloped++;

      if (minorsDeveloped < 2) score -= 30;
    }

    // Castling bonus
    Square kingSq = BB::lsb(pos.pieces(BLACK, KING));
    if (kingSq == G8 || kingSq == C8) {
      score += 40;  // Big bonus for castling
    }

    // Center pawn bonus (e5/d5) - INCREASED to encourage central play
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

    // Check if file is open (no pawns) or semi-open (no our pawns)
    bool hasOurPawns = (ourPawns & fileMask) != 0;
    bool hasEnemyPawns = (enemyPawns & fileMask) != 0;

    if (!hasOurPawns && !hasEnemyPawns) {
      // Rook on open file: +25 bonus
      score += 25;
    } else if (!hasOurPawns && hasEnemyPawns) {
      // Rook on semi-open file (attacking enemy pawns): +15 bonus
      score += 15;
    }

    // Bonus for rook on 7th rank (or 2nd rank for black)
    int rank = rankOf(sq);
    if ((c == WHITE && rank == RANK_7) || (c == BLACK && rank == RANK_2)) {
      score += 20;
    }

    // Rook behind passed pawn (Tarrasch principle)
    // Check both our passed pawns and enemy passed pawns on the same file
    Bitboard allPawnsOnFile = (ourPawns | enemyPawns) & fileMask;
    Bitboard pawnIter = allPawnsOnFile;
    while (pawnIter) {
      Square pawnSq = BB::popLsb(pawnIter);
      int pawnRank = rankOf(pawnSq);
      Color pawnColor = (ourPawns & BB::squareBB(pawnSq)) ? c : ~c;

      // Is this pawn a passed pawn?
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
        // It's a passed pawn. Is our rook behind it?
        bool rookBehind = false;
        if (pawnColor == WHITE)
          rookBehind = rank < pawnRank;  // Rook on lower rank = behind
        else
          rookBehind = rank > pawnRank;  // Rook on higher rank = behind

        if (rookBehind) {
          if (pawnColor == c)
            score += 20;  // Rook behind our own passer
          else
            score += 15;  // Rook behind enemy passer (restrains it)
        }
      }
    }
  }

  return score;
}

static int evaluateBishops(const Position& pos, Color c) {
  int score = 0;
  Bitboard bishops = pos.pieces(c, BISHOP);

  // Bishop pair bonus: +30
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

    // Knight outpost detection:
    // - On 4th, 5th, or 6th rank (for white) / 5th, 4th, 3rd rank (for black)
    // - Defended by our pawn
    // - Cannot be attacked by enemy pawns

    bool isOutpostRank = false;
    if (c == WHITE && (rank == RANK_4 || rank == RANK_5 || rank == RANK_6)) {
      isOutpostRank = true;
    } else if (c == BLACK && (rank == RANK_5 || rank == RANK_4 || rank == RANK_3)) {
      isOutpostRank = true;
    }

    if (isOutpostRank) {
      // Check if defended by our pawn
      Bitboard defenderMask = BB::pawnAttacks(~c, sq);  // Squares from which our pawns would attack this square
      bool defendedByPawn = (defenderMask & ourPawns) != 0;

      if (defendedByPawn) {
        // Check if enemy pawns can attack this square
        bool canBeAttacked = false;

        // Check adjacent files for enemy pawns that could advance to attack
        if (c == WHITE) {
          // For white knight, check if black pawns on adjacent files ahead can attack
          for (int r = rank; r < 8; ++r) {
            if (file > 0 && (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 && (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        } else {
          // For black knight, check if white pawns on adjacent files ahead can attack
          for (int r = rank; r >= 0; --r) {
            if (file > 0 && (enemyPawns & BB::squareBB(r * 8 + file - 1))) {
              canBeAttacked = true;
              break;
            }
            if (file < 7 && (enemyPawns & BB::squareBB(r * 8 + file + 1))) {
              canBeAttacked = true;
              break;
            }
          }
        }

        if (!canBeAttacked) {
          // This is a true outpost! Bonus increases for central files
          int outpostBonus = 25;
          if (file >= 2 && file <= 5) {
            outpostBonus += 10;  // Extra bonus for central outpost
          }
          score += outpostBonus;
        }
      }
    }
  }

  return score;
}

// Manhattan distance from center (for mop-up eval)
static int centerDistance(Square sq) {
  int file = fileOf(sq);
  int rank = rankOf(sq);
  int fileDist = std::max(3 - file, file - 4);
  int rankDist = std::max(3 - rank, rank - 4);
  return fileDist + rankDist;
}

// Chebyshev distance between two squares
static int kingDistance(Square a, Square b) {
  return std::max(std::abs(fileOf(a) - fileOf(b)),
                  std::abs(rankOf(a) - rankOf(b)));
}

// Mop-up bonus: when winning with big material advantage, drive enemy king
// to corner and bring our king close. Only active in endgames.
static int evaluateMopUp(const Position& pos, int materialBalance, int phase) {
  // Only kick in when material advantage > ~400cp and it's an endgame
  if (phase > 128 || std::abs(materialBalance) < 400) return 0;

  Color winner = materialBalance > 0 ? WHITE : BLACK;
  Square winnerKing = BB::lsb(pos.pieces(winner, KING));
  Square loserKing = BB::lsb(pos.pieces(~winner, KING));

  // Push losing king to corner + bring winning king close
  int bonus = centerDistance(loserKing) * 10 + (7 - kingDistance(winnerKing, loserKing)) * 5;

  return winner == WHITE ? bonus : -bonus;
}

// King-passer proximity: in endgames, reward our king being close to our
// passed pawns and penalize when the enemy king is close to our passers.
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

    // Build passed pawn mask
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
      // This is a passed pawn — evaluate king distances
      int ourDist = kingDistance(ourKing, sq);
      int theirDist = kingDistance(theirKing, sq);

      // Bonus for our king being close (escorting the passer)
      score += (7 - ourDist) * 3;

      // Bonus when their king is far (can't stop the passer)
      score += theirDist * 3;
    }
  }

  return score;
}

}  // anonymous namespace

namespace Eval {
int evaluate(const Position& pos) {
  // Material evaluation
  int material = pos.materialCount(WHITE) - pos.materialCount(BLACK);

  int positional = 0;

  // Evaluate white pieces
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

  int kingPositionalMG = 0;  // Middlegame king PST
  int kingPositionalEG = 0;  // Endgame king PST

  Bitboard kings = pos.pieces(WHITE, KING);
  while (kings) {
    Square sq = BB::popLsb(kings);
    kingPositionalMG += kingMiddle[sq];
    kingPositionalEG += kingEndgame[sq];
  }

  // Evaluate black pieces (flip board)
  pawns = pos.pieces(BLACK, PAWN);
  while (pawns) {
    Square sq = BB::popLsb(pawns);
    positional -= pawn[sq ^ 56];  // Flip rank
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

  // Add advanced evaluation features
  int pawnStructure =
      evaluatePawnStructure(pos, WHITE) - evaluatePawnStructure(pos, BLACK);
  int kingSafety =
      evaluateKingSafety(pos, WHITE) - evaluateKingSafety(pos, BLACK);
  int mobility = evaluateMobility(pos, WHITE) - evaluateMobility(pos, BLACK);
  int development =
      evaluateDevelopment(pos, WHITE) - evaluateDevelopment(pos, BLACK);
  int rookScore = evaluateRooks(pos, WHITE) - evaluateRooks(pos, BLACK);
  int bishopScore = evaluateBishops(pos, WHITE) - evaluateBishops(pos, BLACK);
  int knightScore = evaluateKnights(pos, WHITE) - evaluateKnights(pos, BLACK);
  int kingPawnProx =
      evaluateKingPawnProximity(pos, WHITE) - evaluateKingPawnProximity(pos, BLACK);

  // Tapered evaluation - blend opening/endgame scores based on game phase
  int phase = getGamePhase(pos);  // 0 (endgame) to 256 (opening)

  // Opening scores - full weight (development matters most in opening!)
  int openingScore = material + positional + kingPositionalMG + mobility +
                     kingSafety + pawnStructure + development + rookScore +
                     bishopScore + knightScore;

  // Endgame scores - adjust weights
  // In endgame: use endgame king PST, reduce positional/mobility, reduce king
  // safety, increase pawn structure, ignore development, increase rook value,
  // add king-pawn proximity (king escorts passers)
  int endgameScore = material + (positional / 2) + kingPositionalEG +
                     (mobility / 2) + (kingSafety / 4) +
                     (pawnStructure * 3 / 2) + (rookScore * 3 / 2) +
                     bishopScore + knightScore + kingPawnProx;

  // Interpolate between opening and endgame
  int score = (openingScore * phase + endgameScore * (256 - phase)) / 256;

  // Mop-up: when winning big in endgame, drive enemy king to corner
  score += evaluateMopUp(pos, material, phase);

  // Return from perspective of side to move
  return (pos.sideToMove() == WHITE) ? score : -score;
}
}  // namespace Eval
