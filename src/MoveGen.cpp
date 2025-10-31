#include "MoveGen.h"

#include <iostream>

#include "Bitboard.h"
#include "Magic.h"

namespace MoveGen {

// Add pawn moves for a single pawn
template <Color Us>
void generatePawnMoves(const Position& pos, std::vector<Move>& moves,
                       Square from) {
  constexpr Color Them = (Us == WHITE) ? BLACK : WHITE;
  constexpr int Up = (Us == WHITE) ? 8 : -8;
  constexpr int PromotionRank = (Us == WHITE) ? RANK_7 : RANK_2;

  Square to = Square(from + Up);
  Bitboard occupied = pos.occupied();

  // Single push
  if (!BB::testBit(occupied, to)) {
    if (rankOf(from) == PromotionRank) {
      // Promotions
      moves.push_back(makePromotion(from, to, QUEEN));
      moves.push_back(makePromotion(from, to, ROOK));
      moves.push_back(makePromotion(from, to, BISHOP));
      moves.push_back(makePromotion(from, to, KNIGHT));
    } else {
      moves.push_back(makeMove(from, to));

      // Double push
      constexpr int StartRank = (Us == WHITE) ? RANK_2 : RANK_7;
      if (rankOf(from) == StartRank) {
        Square doubleTo = Square(from + 2 * Up);
        if (!BB::testBit(occupied, doubleTo)) {
          moves.push_back(makeMove(from, doubleTo));
        }
      }
    }
  }

  // Captures
  Bitboard attacks = BB::pawnAttacks(Us, from);
  Bitboard captureTargets = attacks & pos.pieces(Them);

  while (captureTargets) {
    to = BB::popLsb(captureTargets);
    if (rankOf(from) == PromotionRank) {
      moves.push_back(makePromotion(from, to, QUEEN));
      moves.push_back(makePromotion(from, to, ROOK));
      moves.push_back(makePromotion(from, to, BISHOP));
      moves.push_back(makePromotion(from, to, KNIGHT));
    } else {
      moves.push_back(makeMove(from, to));
    }
  }

  // En passant
  Square epSq = pos.enPassantSquare();
  if (epSq != NO_SQUARE && (attacks & BB::squareBB(epSq))) {
    moves.push_back(makeEnPassant(from, epSq));
  }
}

// Generate moves for a piece type
template <PieceType Pt>
void generatePieceMoves(const Position& pos, std::vector<Move>& moves, Color us,
                        Bitboard pieces) {
  Bitboard targets =
      ~pos.pieces(us);  // Can move to empty squares or enemy pieces
  Bitboard occupied = pos.occupied();

  while (pieces) {
    Square from = BB::popLsb(pieces);
    Bitboard attacks;

    if constexpr (Pt == KNIGHT) {
      attacks = BB::knightAttacks(from);
    } else if constexpr (Pt == BISHOP) {
      attacks = Magic::bishopAttacks(from, occupied);
    } else if constexpr (Pt == ROOK) {
      attacks = Magic::rookAttacks(from, occupied);
    } else if constexpr (Pt == QUEEN) {
      attacks = Magic::queenAttacks(from, occupied);
    } else if constexpr (Pt == KING) {
      attacks = BB::kingAttacks(from);
    }

    attacks &= targets;

    while (attacks) {
      Square to = BB::popLsb(attacks);
      moves.push_back(makeMove(from, to));
    }
  }
}

// Generate castling moves
template <Color Us>
void generateCastling(const Position& pos, std::vector<Move>& moves) {
  constexpr Color Them = (Us == WHITE) ? BLACK : WHITE;

  if (pos.inCheck()) return;  // Cannot castle out of check

  Bitboard occupied = pos.occupied();
  int rights = pos.castlingRights();

  // Kingside castling
  if (Us == WHITE && (rights & WHITE_OO)) {
    if (!(occupied & ((1ULL << F1) | (1ULL << G1)))) {
      if (!pos.isAttacked(E1, Them) && !pos.isAttacked(F1, Them) &&
          !pos.isAttacked(G1, Them)) {
        moves.push_back(makeCastling(E1, G1));
      }
    }
  } else if (Us == BLACK && (rights & BLACK_OO)) {
    if (!(occupied & ((1ULL << F8) | (1ULL << G8)))) {
      if (!pos.isAttacked(E8, Them) && !pos.isAttacked(F8, Them) &&
          !pos.isAttacked(G8, Them)) {
        moves.push_back(makeCastling(E8, G8));
      }
    }
  }

  // Queenside castling
  if (Us == WHITE && (rights & WHITE_OOO)) {
    if (!(occupied & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1)))) {
      if (!pos.isAttacked(E1, Them) && !pos.isAttacked(D1, Them) &&
          !pos.isAttacked(C1, Them)) {
        moves.push_back(makeCastling(E1, C1));
      }
    }
  } else if (Us == BLACK && (rights & BLACK_OOO)) {
    if (!(occupied & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8)))) {
      if (!pos.isAttacked(E8, Them) && !pos.isAttacked(D8, Them) &&
          !pos.isAttacked(C8, Them)) {
        moves.push_back(makeCastling(E8, C8));
      }
    }
  }
}

std::vector<Move> generatePseudoLegalMoves(const Position& pos) {
  std::vector<Move> moves;
  moves.reserve(128);

  Color us = pos.sideToMove();

  if (us == WHITE) {
    // Pawns
    Bitboard pawns = pos.pieces(WHITE, PAWN);
    while (pawns) {
      Square sq = BB::popLsb(pawns);
      generatePawnMoves<WHITE>(pos, moves, sq);
    }

    // Pieces
    generatePieceMoves<KNIGHT>(pos, moves, WHITE, pos.pieces(WHITE, KNIGHT));
    generatePieceMoves<BISHOP>(pos, moves, WHITE, pos.pieces(WHITE, BISHOP));
    generatePieceMoves<ROOK>(pos, moves, WHITE, pos.pieces(WHITE, ROOK));
    generatePieceMoves<QUEEN>(pos, moves, WHITE, pos.pieces(WHITE, QUEEN));
    generatePieceMoves<KING>(pos, moves, WHITE, pos.pieces(WHITE, KING));

    // Castling
    generateCastling<WHITE>(pos, moves);

  } else {
    // Pawns
    Bitboard pawns = pos.pieces(BLACK, PAWN);
    while (pawns) {
      Square sq = BB::popLsb(pawns);
      generatePawnMoves<BLACK>(pos, moves, sq);
    }

    // Pieces
    generatePieceMoves<KNIGHT>(pos, moves, BLACK, pos.pieces(BLACK, KNIGHT));
    generatePieceMoves<BISHOP>(pos, moves, BLACK, pos.pieces(BLACK, BISHOP));
    generatePieceMoves<ROOK>(pos, moves, BLACK, pos.pieces(BLACK, ROOK));
    generatePieceMoves<QUEEN>(pos, moves, BLACK, pos.pieces(BLACK, QUEEN));
    generatePieceMoves<KING>(pos, moves, BLACK, pos.pieces(BLACK, KING));

    // Castling
    generateCastling<BLACK>(pos, moves);
  }

  return moves;
}

bool isLegal(Position& pos, Move move) {
  // Make the move
  Color us = pos.sideToMove();
  pos.makeMove(move);

  // Check if our king is in check (illegal)
  bool legal = !pos.isAttacked(BB::lsb(pos.pieces(us, KING)), pos.sideToMove());

  // Unmake the move
  pos.unmakeMove();

  return legal;
}

std::vector<Move> generateLegalMoves(Position& pos) {
  std::vector<Move> pseudoMoves = generatePseudoLegalMoves(pos);
  std::vector<Move> legalMoves;
  legalMoves.reserve(pseudoMoves.size());

  for (Move move : pseudoMoves) {
    if (isLegal(pos, move)) {
      legalMoves.push_back(move);
    }
  }

  return legalMoves;
}

std::vector<Move> generateCaptures(Position& pos) {
  std::vector<Move> pseudoMoves = generatePseudoLegalMoves(pos);
  std::vector<Move> captures;

  for (Move move : pseudoMoves) {
    Square to = toSquare(move);
    // Check if it's a capture
    if (pos.pieceAt(to) != NO_PIECE || moveType(move) == EN_PASSANT) {
      // Verify it's legal
      if (isLegal(pos, move)) {
        captures.push_back(move);
      }
    }
  }

  return captures;
}

uint64_t perft(Position& pos, int depth) {
  if (depth == 0) return 1;

  std::vector<Move> moves = generateLegalMoves(pos);

  if (depth == 1) return moves.size();

  uint64_t nodes = 0;
  for (Move move : moves) {
    pos.makeMove(move);
    nodes += perft(pos, depth - 1);
    pos.unmakeMove();
  }

  return nodes;
}

void perftDivide(Position& pos, int depth) {
  std::vector<Move> moves = generateLegalMoves(pos);
  uint64_t totalNodes = 0;

  std::cout << "\nPerft Divide (depth " << depth << "):\n";
  std::cout << "--------------------------------\n";

  for (Move move : moves) {
    pos.makeMove(move);
    uint64_t nodes = (depth > 1) ? perft(pos, depth - 1) : 1;
    pos.unmakeMove();

    std::cout << moveToString(move) << ": " << nodes << "\n";
    totalNodes += nodes;
  }

  std::cout << "--------------------------------\n";
  std::cout << "Total nodes: " << totalNodes << "\n";
}

}  // namespace MoveGen
