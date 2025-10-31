#include "Position.h"

#include <cctype>
#include <iostream>
#include <sstream>

#include "Bitboard.h"
#include "Magic.h"
#include "Zobrist.h"

Position::Position() {
  clear();
  setFromFEN(STARTING_FEN);
}

void Position::clear() {
  for (int i = 0; i < 6; ++i) byType[i] = 0;
  for (int i = 0; i < 2; ++i) byColor[i] = 0;
  for (int i = 0; i < 64; ++i) board[i] = NO_PIECE;

  stm = WHITE;
  castling = NO_CASTLING;
  epSquare = NO_SQUARE;
  halfmoves = 0;
  fullmoves = 1;
  positionHash = 0;
  history.clear();
}

void Position::putPiece(Piece pc, Square sq) {
  Color c = colorOf(pc);
  PieceType pt = typeOf(pc);

  byType[pt] |= BB::squareBB(sq);
  byColor[c] |= BB::squareBB(sq);
  board[sq] = pc;
  positionHash ^= Zobrist::psq[pc][sq];
}

void Position::removePiece(Square sq) {
  Piece pc = board[sq];
  if (pc == NO_PIECE) return;

  Color c = colorOf(pc);
  PieceType pt = typeOf(pc);

  byType[pt] &= ~BB::squareBB(sq);
  byColor[c] &= ~BB::squareBB(sq);
  positionHash ^= Zobrist::psq[pc][sq];
  board[sq] = NO_PIECE;
}

void Position::movePiece(Square from, Square to) {
  Piece pc = board[from];
  removePiece(from);
  putPiece(pc, to);
}

bool Position::setFromFEN(const std::string& fen) {
  clear();

  std::istringstream iss(fen);
  std::string position, turn, castlingStr, epStr;
  int halfmoveStr, fullmoveStr;

  iss >> position >> turn >> castlingStr >> epStr >> halfmoveStr >> fullmoveStr;

  // Parse position
  int rank = 7, file = 0;
  for (char ch : position) {
    if (ch == '/') {
      rank--;
      file = 0;
    } else if (isdigit(ch)) {
      file += ch - '0';
    } else {
      Color c = isupper(ch) ? WHITE : BLACK;
      Piece pc = NO_PIECE;

      switch (tolower(ch)) {
        case 'p':
          pc = makePiece(c, PAWN);
          break;
        case 'n':
          pc = makePiece(c, KNIGHT);
          break;
        case 'b':
          pc = makePiece(c, BISHOP);
          break;
        case 'r':
          pc = makePiece(c, ROOK);
          break;
        case 'q':
          pc = makePiece(c, QUEEN);
          break;
        case 'k':
          pc = makePiece(c, KING);
          break;
        default:
          return false;
      }

      putPiece(pc, makeSquare(file, rank));
      file++;
    }
  }

  // Parse side to move
  stm = (turn == "w") ? WHITE : BLACK;

  // Parse castling rights
  castling = NO_CASTLING;
  for (char ch : castlingStr) {
    switch (ch) {
      case 'K':
        castling |= WHITE_OO;
        break;
      case 'Q':
        castling |= WHITE_OOO;
        break;
      case 'k':
        castling |= BLACK_OO;
        break;
      case 'q':
        castling |= BLACK_OOO;
        break;
    }
  }

  // Parse en passant square
  epSquare = stringToSquare(epStr);

  // Parse move counters
  halfmoves = halfmoveStr;
  fullmoves = fullmoveStr;

  // Compute hash for side to move
  if (stm == BLACK) {
    positionHash ^= Zobrist::sideToMove;
  }

  // Compute hash for castling rights
  positionHash ^= Zobrist::castling[castling];

  // Compute hash for en passant
  if (epSquare != NO_SQUARE) {
    positionHash ^= Zobrist::enpassant[fileOf(epSquare)];
  }

  return true;
}

std::string Position::getFEN() const {
  std::ostringstream oss;

  // Position
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      Square sq = makeSquare(file, rank);
      Piece pc = board[sq];

      if (pc == NO_PIECE) {
        empty++;
      } else {
        if (empty > 0) {
          oss << empty;
          empty = 0;
        }

        Color c = colorOf(pc);
        PieceType pt = typeOf(pc);
        const char* pieces = "pnbrqk";
        char ch = pieces[pt];
        oss << (c == WHITE ? char(toupper(ch)) : ch);
      }
    }
    if (empty > 0) oss << empty;
    if (rank > 0) oss << '/';
  }

  // Side to move
  oss << (stm == WHITE ? " w " : " b ");

  // Castling rights
  if (castling == NO_CASTLING) {
    oss << "- ";
  } else {
    if (castling & WHITE_OO) oss << 'K';
    if (castling & WHITE_OOO) oss << 'Q';
    if (castling & BLACK_OO) oss << 'k';
    if (castling & BLACK_OOO) oss << 'q';
    oss << ' ';
  }

  // En passant
  oss << squareToString(epSquare) << ' ';

  // Move counters
  oss << halfmoves << ' ' << fullmoves;

  return oss.str();
}

Piece Position::pieceAt(Square sq) const { return board[sq]; }

void Position::makeMove(Move move) {
  // Save state for unmake
  StateInfo state;
  state.move = move;
  state.captured = NO_PIECE;
  state.castling = castling;
  state.epSquare = epSquare;
  state.halfmoves = halfmoves;
  state.hash = positionHash;
  history.push_back(state);

  Square from = fromSquare(move);
  Square to = toSquare(move);
  Piece pc = board[from];
  PieceType pt = typeOf(pc);
  Color us = stm;

  // Remove old en passant from hash
  if (epSquare != NO_SQUARE) {
    positionHash ^= Zobrist::enpassant[fileOf(epSquare)];
  }

  // Remove old castling from hash
  positionHash ^= Zobrist::castling[castling];

  // Reset en passant
  epSquare = NO_SQUARE;

  // Increment halfmove clock
  halfmoves++;

  // Handle different move types
  int mt = moveType(move);

  if (mt == NORMAL_MOVE) {
    // Capture?
    if (board[to] != NO_PIECE) {
      history.back().captured = board[to];
      removePiece(to);
      halfmoves = 0;  // Reset on capture
    }

    // Reset halfmove clock on pawn move
    if (pt == PAWN) halfmoves = 0;

    // Double pawn push - set en passant square
    if (pt == PAWN && std::abs(from - to) == 16) {
      epSquare = Square(from + (us == WHITE ? 8 : -8));
    }

    movePiece(from, to);

  } else if (mt == PROMOTION) {
    // Capture?
    if (board[to] != NO_PIECE) {
      history.back().captured = board[to];
      removePiece(to);
    }

    removePiece(from);
    putPiece(makePiece(us, promotionType(move)), to);
    halfmoves = 0;

  } else if (mt == EN_PASSANT) {
    Square capturedSq = Square(to - (us == WHITE ? 8 : -8));
    history.back().captured = board[capturedSq];
    removePiece(capturedSq);
    movePiece(from, to);
    halfmoves = 0;

  } else if (mt == CASTLING) {
    // Move king
    movePiece(from, to);

    // Move rook
    if (to > from) {  // Kingside
      Square rookFrom = makeSquare(FILE_H, rankOf(from));
      Square rookTo = makeSquare(FILE_F, rankOf(from));
      movePiece(rookFrom, rookTo);
    } else {  // Queenside
      Square rookFrom = makeSquare(FILE_A, rankOf(from));
      Square rookTo = makeSquare(FILE_D, rankOf(from));
      movePiece(rookFrom, rookTo);
    }
  }

  // Update castling rights
  if (pt == KING) {
    if (us == WHITE) {
      castling &= ~(WHITE_OO | WHITE_OOO);
    } else {
      castling &= ~(BLACK_OO | BLACK_OOO);
    }
  }

  if (pt == ROOK || history.back().captured != NO_PIECE) {
    if (from == A1 || to == A1) castling &= ~WHITE_OOO;
    if (from == H1 || to == H1) castling &= ~WHITE_OO;
    if (from == A8 || to == A8) castling &= ~BLACK_OOO;
    if (from == H8 || to == H8) castling &= ~BLACK_OO;
  }

  // Add new castling to hash
  positionHash ^= Zobrist::castling[castling];

  // Add new en passant to hash
  if (epSquare != NO_SQUARE) {
    positionHash ^= Zobrist::enpassant[fileOf(epSquare)];
  }

  // Switch side
  stm = ~stm;
  if (stm == WHITE) fullmoves++;

  // Update hash for side to move (always flip)
  positionHash ^= Zobrist::sideToMove;
}

void Position::unmakeMove() {
  if (history.empty()) return;

  // Get last move info
  StateInfo state = history.back();
  history.pop_back();

  Move move = state.move;
  Square from = fromSquare(move);
  Square to = toSquare(move);
  int mt = moveType(move);

  // Switch side back
  stm = ~stm;
  Color us = stm;

  // Restore state (including hash)
  castling = state.castling;
  epSquare = state.epSquare;
  halfmoves = state.halfmoves;
  positionHash = state.hash;
  if (stm == BLACK) fullmoves--;  // Only decrement when undoing a black move

  // Undo the move
  if (mt == NORMAL_MOVE) {
    // Move piece back
    Piece pc = board[to];
    removePiece(to);
    putPiece(pc, from);

    // Restore captured piece
    if (state.captured != NO_PIECE) {
      putPiece(state.captured, to);
    }

  } else if (mt == PROMOTION) {
    // Remove promoted piece, restore pawn
    removePiece(to);
    putPiece(makePiece(us, PAWN), from);

    // Restore captured piece
    if (state.captured != NO_PIECE) {
      putPiece(state.captured, to);
    }

  } else if (mt == EN_PASSANT) {
    // Move pawn back
    Piece pc = board[to];
    removePiece(to);
    putPiece(pc, from);

    // Restore captured pawn
    Square capturedSq = Square(to - (us == WHITE ? 8 : -8));
    putPiece(state.captured, capturedSq);

  } else if (mt == CASTLING) {
    // Move king back
    Piece king = board[to];
    removePiece(to);
    putPiece(king, from);

    // Move rook back
    if (to > from) {  // Kingside
      Square rookFrom = makeSquare(FILE_H, rankOf(from));
      Square rookTo = makeSquare(FILE_F, rankOf(from));
      Piece rook = board[rookTo];
      removePiece(rookTo);
      putPiece(rook, rookFrom);
    } else {  // Queenside
      Square rookFrom = makeSquare(FILE_A, rankOf(from));
      Square rookTo = makeSquare(FILE_D, rankOf(from));
      Piece rook = board[rookTo];
      removePiece(rookTo);
      putPiece(rook, rookFrom);
    }
  }
}

void Position::makeNullMove() {
  // Save state for unmake
  StateInfo state;
  state.move = 0;  // Null move
  state.captured = NO_PIECE;
  state.castling = castling;
  state.epSquare = epSquare;
  state.halfmoves = halfmoves;
  state.hash = positionHash;
  history.push_back(state);

  // Remove en passant from hash
  if (epSquare != NO_SQUARE) {
    positionHash ^= Zobrist::enpassant[fileOf(epSquare)];
  }
  epSquare = NO_SQUARE;

  // Switch side to move
  stm = ~stm;
  positionHash ^= Zobrist::sideToMove;

  // Increment halfmove counter
  halfmoves++;
  if (stm == WHITE) fullmoves++;
}

void Position::unmakeNullMove() {
  if (history.empty()) return;

  // Get last state
  StateInfo state = history.back();
  history.pop_back();

  // Restore state
  stm = ~stm;
  castling = state.castling;
  epSquare = state.epSquare;
  halfmoves = state.halfmoves;
  positionHash = state.hash;

  if (stm == BLACK) fullmoves--;
}

bool Position::inCheck() const {
  Square kingSq = BB::lsb(pieces(stm, KING));
  return isAttacked(kingSq, ~stm);
}

bool Position::isAttacked(Square sq, Color attackerColor) const {
  // Pawn attacks
  if (BB::pawnAttacks(~attackerColor, sq) & pieces(attackerColor, PAWN))
    return true;

  // Knight attacks
  if (BB::knightAttacks(sq) & pieces(attackerColor, KNIGHT)) return true;

  // King attacks
  if (BB::kingAttacks(sq) & pieces(attackerColor, KING)) return true;

  Bitboard occ = occupied();

  // Bishop/Queen attacks
  if (Magic::bishopAttacks(sq, occ) &
      (pieces(attackerColor, BISHOP) | pieces(attackerColor, QUEEN)))
    return true;

  // Rook/Queen attacks
  if (Magic::rookAttacks(sq, occ) &
      (pieces(attackerColor, ROOK) | pieces(attackerColor, QUEEN)))
    return true;

  return false;
}

Bitboard Position::attacksTo(Square sq) const {
  Bitboard attacks = 0;
  Bitboard occ = occupied();

  // Pawns
  attacks |= BB::pawnAttacks(WHITE, sq) & pieces(BLACK, PAWN);
  attacks |= BB::pawnAttacks(BLACK, sq) & pieces(WHITE, PAWN);

  // Knights
  attacks |=
      BB::knightAttacks(sq) & (pieces(WHITE, KNIGHT) | pieces(BLACK, KNIGHT));

  // Kings
  attacks |= BB::kingAttacks(sq) & (pieces(WHITE, KING) | pieces(BLACK, KING));

  // Bishops and queens
  Bitboard bishopAttackers = pieces(WHITE, BISHOP) | pieces(BLACK, BISHOP) |
                             pieces(WHITE, QUEEN) | pieces(BLACK, QUEEN);
  attacks |= Magic::bishopAttacks(sq, occ) & bishopAttackers;

  // Rooks and queens
  Bitboard rookAttackers = pieces(WHITE, ROOK) | pieces(BLACK, ROOK) |
                           pieces(WHITE, QUEEN) | pieces(BLACK, QUEEN);
  attacks |= Magic::rookAttacks(sq, occ) & rookAttackers;

  return attacks;
}

int Position::materialCount(Color c) const {
  int material = 0;
  material += BB::popCount(pieces(c, PAWN)) * 100;
  material += BB::popCount(pieces(c, KNIGHT)) * 320;
  material += BB::popCount(pieces(c, BISHOP)) * 330;
  material += BB::popCount(pieces(c, ROOK)) * 500;
  material += BB::popCount(pieces(c, QUEEN)) * 900;
  return material;
}

void Position::print() const {
  std::cout << "\n";
  for (int rank = 7; rank >= 0; --rank) {
    std::cout << (rank + 1) << " ";
    for (int file = 0; file < 8; ++file) {
      Square sq = makeSquare(file, rank);
      Piece pc = board[sq];

      if (pc == NO_PIECE) {
        std::cout << ". ";
      } else {
        const char* pieces = "PNBRQK";
        char ch = pieces[typeOf(pc)];
        if (colorOf(pc) == BLACK) ch = tolower(ch);
        std::cout << ch << ' ';
      }
    }
    std::cout << "\n";
  }
  std::cout << "  a b c d e f g h\n";
  std::cout << "FEN: " << getFEN() << "\n";
}

// X-ray attacks - attacks through blocking pieces
Bitboard Position::xrayRookAttacks(Square sq, Bitboard blockers,
                                   Bitboard occ) const {
  Bitboard attacks = Magic::rookAttacks(sq, occ);
  blockers &= attacks;
  return attacks ^ Magic::rookAttacks(sq, occ ^ blockers);
}

Bitboard Position::xrayBishopAttacks(Square sq, Bitboard blockers,
                                     Bitboard occ) const {
  Bitboard attacks = Magic::bishopAttacks(sq, occ);
  blockers &= attacks;
  return attacks ^ Magic::bishopAttacks(sq, occ ^ blockers);
}

// Find all pieces pinned to the king
Bitboard Position::pinnedPieces(Color c) const {
  Bitboard pinned = 0;
  Square kingSq = BB::lsb(pieces(c, KING));
  Bitboard pinners = 0;
  Bitboard occ = occupied();

  // Find potential pinning pieces (enemy sliders)
  Color them = ~c;
  Bitboard rookPinners = pieces(them, ROOK) | pieces(them, QUEEN);
  Bitboard bishopPinners = pieces(them, BISHOP) | pieces(them, QUEEN);

  // Check for rook/queen pins
  Bitboard rookXray = xrayRookAttacks(kingSq, pieces(c), occ);
  pinners = rookXray & rookPinners;
  while (pinners) {
    Square pinnerSq = BB::popLsb(pinners);
    Bitboard between = BB::betweenBB(kingSq, pinnerSq) & pieces(c);
    if (BB::popCount(between) == 1) {
      pinned |= between;
    }
  }

  // Check for bishop/queen pins
  Bitboard bishopXray = xrayBishopAttacks(kingSq, pieces(c), occ);
  pinners = bishopXray & bishopPinners;
  while (pinners) {
    Square pinnerSq = BB::popLsb(pinners);
    Bitboard between = BB::betweenBB(kingSq, pinnerSq) & pieces(c);
    if (BB::popCount(between) == 1) {
      pinned |= between;
    }
  }

  return pinned;
}

bool Position::isPinned(Square sq, Color c) const {
  return (pinnedPieces(c) & BB::squareBB(sq)) != 0;
}

// Static Exchange Evaluation - evaluate capture sequences
int Position::see(Move move) const {
  static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};

  Square from = fromSquare(move);
  Square to = toSquare(move);
  Piece attacker = board[from];
  Piece captured = board[to];

  // Handle en passant
  if (moveType(move) == EN_PASSANT) {
    captured = makePiece(~colorOf(attacker), PAWN);
  }

  if (captured == NO_PIECE && moveType(move) != EN_PASSANT) {
    return 0;  // Not a capture
  }

  // Use a gain list approach (non-mutating)
  int gain[32];  // Max possible capture sequence depth
  int depth = 0;

  Bitboard mayXray =
      (pieces(WHITE, PAWN) | pieces(BLACK, PAWN) | pieces(WHITE, BISHOP) |
       pieces(BLACK, BISHOP) | pieces(WHITE, ROOK) | pieces(BLACK, ROOK) |
       pieces(WHITE, QUEEN) | pieces(BLACK, QUEEN));

  Bitboard fromSet = BB::squareBB(from);
  Bitboard occ = occupied();
  Bitboard attackers = attacksTo(to) & occ;

  Color side = colorOf(attacker);
  PieceType attackerType = typeOf(attacker);

  // Initial capture gain
  gain[depth] = pieceValues[typeOf(captured)];

  do {
    depth++;
    gain[depth] = pieceValues[attackerType] - gain[depth - 1];

    // Worst case: we lose the capturing piece
    if (std::max(-gain[depth - 1], gain[depth]) < 0) {
      break;  // Prune: losing no matter what
    }

    // Remove the attacker from occupied squares
    attackers ^= fromSet;
    occ ^= fromSet;

    // Add X-ray attackers if piece was blocking
    if (fromSet & mayXray) {
      attackers |= (Magic::bishopAttacks(to, occ) &
                    (pieces(WHITE, BISHOP) | pieces(BLACK, BISHOP) |
                     pieces(WHITE, QUEEN) | pieces(BLACK, QUEEN))) &
                   occ;
      attackers |= (Magic::rookAttacks(to, occ) &
                    (pieces(WHITE, ROOK) | pieces(BLACK, ROOK) |
                     pieces(WHITE, QUEEN) | pieces(BLACK, QUEEN))) &
                   occ;
    }

    // Switch sides
    side = ~side;

    // Find next least valuable attacker for this side
    attackers &= occ;  // Remove already used pieces
    fromSet = 0;
    attackerType = NO_PIECE_TYPE;

    for (PieceType pt = PAWN; pt <= KING; pt = PieceType(pt + 1)) {
      fromSet = attackers & pieces(side, pt);
      if (fromSet) {
        attackerType = pt;
        fromSet = BB::squareBB(BB::lsb(fromSet));  // Pick first one
        break;
      }
    }

  } while (fromSet);

  // Minimax over the gain list
  while (--depth) {
    gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
  }

  return gain[0];
}

// Recursive SEE - simulate capture sequence (DEPRECATED - use see() instead)
int Position::seeCapture(Square, Color, Piece captured, Piece) const {
  // This function is now unused but kept for backward compatibility
  static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
  return pieceValues[typeOf(captured)];
}
