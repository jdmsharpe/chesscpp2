#include "Tablebase.h"

#include <iostream>

#include "Bitboard.h"
#include "Logger.h"
#include "Magic.h"
#include "MoveGen.h"

// Define attack functions before including Fathom headers
// This allows Fathom to use our existing optimized attack generation
#define TB_KING_ATTACKS(sq) BB::kingAttacks(static_cast<Square>(sq))
#define TB_KNIGHT_ATTACKS(sq) BB::knightAttacks(static_cast<Square>(sq))
#define TB_ROOK_ATTACKS(sq, occ) Magic::rookAttacks(static_cast<Square>(sq), occ)
#define TB_BISHOP_ATTACKS(sq, occ) \
  Magic::bishopAttacks(static_cast<Square>(sq), occ)
#define TB_PAWN_ATTACKS(sq, color) \
  BB::pawnAttacks(static_cast<Color>(color), static_cast<Square>(sq))

// Disable helper API since we have our own functions
#define TB_NO_HELPER_API

extern "C" {
#include "tbprobe.h"
}

// Static member definitions
bool Tablebase::initialized = false;
int Tablebase::largestPieces = 0;

bool Tablebase::init(const std::string& path) {
  if (path.empty()) {
    Logger::getInstance().info("Tablebase: No path specified");
    return false;
  }

  bool success = tb_init(path.c_str());
  if (success) {
    initialized = true;
    largestPieces = TB_LARGEST;
    if (largestPieces > 0) {
      Logger::getInstance().info("Tablebase: Loaded " +
                                 std::to_string(largestPieces) +
                                 "-piece tablebases from " + path);
    } else {
      Logger::getInstance().warning(
          "Tablebase: No tablebase files found in " + path);
    }
  } else {
    Logger::getInstance().error("Tablebase: Failed to initialize from " + path);
  }
  return success && largestPieces > 0;
}

void Tablebase::free() {
  if (initialized) {
    tb_free();
    initialized = false;
    largestPieces = 0;
  }
}

bool Tablebase::available() { return initialized && largestPieces > 0; }

int Tablebase::maxPieces() { return largestPieces; }

bool Tablebase::canProbe(const Position& pos) {
  if (!available()) return false;

  // Count total pieces
  int pieceCount = BB::popCount(pos.occupied());
  if (pieceCount > largestPieces) return false;

  return true;
}

TBResult Tablebase::probeWDL(const Position& pos) {
  if (!canProbe(pos)) return TB_RESULT_UNKNOWN;

  // WDL probe requires no castling rights and rule50 == 0
  if (pos.castlingRights() != 0) return TB_RESULT_UNKNOWN;
  if (pos.halfmoveClock() != 0) return TB_RESULT_UNKNOWN;

  // Get bitboards for Fathom
  uint64_t white = pos.pieces(WHITE);
  uint64_t black = pos.pieces(BLACK);
  uint64_t kings = pos.pieces(WHITE, KING) | pos.pieces(BLACK, KING);
  uint64_t queens = pos.pieces(WHITE, QUEEN) | pos.pieces(BLACK, QUEEN);
  uint64_t rooks = pos.pieces(WHITE, ROOK) | pos.pieces(BLACK, ROOK);
  uint64_t bishops = pos.pieces(WHITE, BISHOP) | pos.pieces(BLACK, BISHOP);
  uint64_t knights = pos.pieces(WHITE, KNIGHT) | pos.pieces(BLACK, KNIGHT);
  uint64_t pawns = pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN);

  // En passant square (0 if none)
  unsigned ep = 0;
  if (pos.enPassantSquare() != NO_SQUARE) {
    ep = pos.enPassantSquare();
  }

  bool turn = (pos.sideToMove() == WHITE);

  unsigned result = tb_probe_wdl(white, black, kings, queens, rooks, bishops,
                                 knights, pawns, 0,  // rule50
                                 0,                  // castling
                                 ep, turn);

  if (result == TB_RESULT_FAILED) {
    return TB_RESULT_UNKNOWN;
  }

  return static_cast<TBResult>(result);
}

TBProbeResult Tablebase::probeRoot(const Position& pos) {
  TBProbeResult result;
  result.wdl = TB_RESULT_UNKNOWN;
  result.dtz = 0;
  result.bestMove = 0;
  result.success = false;

  if (!canProbe(pos)) return result;

  // Root probe requires no castling rights
  if (pos.castlingRights() != 0) return result;

  // Get bitboards
  uint64_t white = pos.pieces(WHITE);
  uint64_t black = pos.pieces(BLACK);
  uint64_t kings = pos.pieces(WHITE, KING) | pos.pieces(BLACK, KING);
  uint64_t queens = pos.pieces(WHITE, QUEEN) | pos.pieces(BLACK, QUEEN);
  uint64_t rooks = pos.pieces(WHITE, ROOK) | pos.pieces(BLACK, ROOK);
  uint64_t bishops = pos.pieces(WHITE, BISHOP) | pos.pieces(BLACK, BISHOP);
  uint64_t knights = pos.pieces(WHITE, KNIGHT) | pos.pieces(BLACK, KNIGHT);
  uint64_t pawns = pos.pieces(WHITE, PAWN) | pos.pieces(BLACK, PAWN);

  unsigned ep = 0;
  if (pos.enPassantSquare() != NO_SQUARE) {
    ep = pos.enPassantSquare();
  }

  bool turn = (pos.sideToMove() == WHITE);

  // Probe root - this gives us the best move according to DTZ
  unsigned tbResult = tb_probe_root(white, black, kings, queens, rooks, bishops,
                                    knights, pawns,
                                    pos.halfmoveClock(),  // rule50
                                    0,                    // castling
                                    ep, turn,
                                    nullptr);  // No alternative results needed

  if (tbResult == TB_RESULT_FAILED) {
    return result;
  }

  // Check for checkmate/stalemate
  if (tbResult == TB_RESULT_CHECKMATE) {
    result.wdl = TB_RESULT_LOSS;  // We're checkmated
    result.success = true;
    return result;
  }

  if (tbResult == TB_RESULT_STALEMATE) {
    result.wdl = TB_RESULT_DRAW;
    result.success = true;
    return result;
  }

  // Extract WDL and move info
  unsigned wdl = TB_GET_WDL(tbResult);
  unsigned from = TB_GET_FROM(tbResult);
  unsigned to = TB_GET_TO(tbResult);
  unsigned promotes = TB_GET_PROMOTES(tbResult);
  unsigned dtz = TB_GET_DTZ(tbResult);

  result.wdl = static_cast<TBResult>(wdl);
  result.dtz = dtz;
  result.success = true;

  // Convert Fathom move to our Move format
  // We need to find the legal move that matches from/to/promotion
  // Note: generateLegalMoves needs non-const position, so make a copy
  Position posCopy = pos;
  std::vector<Move> legalMoves = MoveGen::generateLegalMoves(posCopy);

  for (Move m : legalMoves) {
    if (fromSquare(m) == static_cast<Square>(from) &&
        toSquare(m) == static_cast<Square>(to)) {
      // Check promotion type if applicable
      if (promotes != TB_PROMOTES_NONE) {
        if (moveType(m) != PROMOTION) continue;

        PieceType promoPiece = promotionType(m);
        bool match = false;
        switch (promotes) {
          case TB_PROMOTES_QUEEN:
            match = (promoPiece == QUEEN);
            break;
          case TB_PROMOTES_ROOK:
            match = (promoPiece == ROOK);
            break;
          case TB_PROMOTES_BISHOP:
            match = (promoPiece == BISHOP);
            break;
          case TB_PROMOTES_KNIGHT:
            match = (promoPiece == KNIGHT);
            break;
        }
        if (!match) continue;
      }

      result.bestMove = m;
      break;
    }
  }

  return result;
}

int Tablebase::wdlToScore(TBResult wdl, int ply) {
  switch (wdl) {
    case TB_RESULT_WIN:
      return TB_WIN_SCORE - ply;  // Prefer faster wins
    case TB_RESULT_CURSED_WIN:
      return TB_CURSED_WIN_SCORE;  // Win but 50-move draw likely
    case TB_RESULT_DRAW:
      return 0;
    case TB_RESULT_BLESSED_LOSS:
      return TB_BLESSED_LOSS_SCORE;  // Loss but can claim 50-move draw
    case TB_RESULT_LOSS:
      return TB_LOSS_SCORE + ply;  // Prefer longer losses
    default:
      return 0;
  }
}
