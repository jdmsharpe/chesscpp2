#pragma once

#include "PST.h"
#include "Types.h"

#include <string>
#include <vector>

// Represents a complete chess position using bitboards
class Position {
 public:
  Position();

  // Initialize from FEN string
  bool setFromFEN(const std::string& fen);

  // Get FEN string
  std::string getFEN() const;

  // Make/unmake moves
  void makeMove(Move move);
  void unmakeMove();

  // Null move (pass) for null move pruning
  void makeNullMove();
  void unmakeNullMove();

  // Board queries
  Piece pieceAt(Square sq) const;
  Bitboard pieces(Color c) const { return byColor[c]; }
  Bitboard pieces(PieceType pt) const { return byType[pt]; }
  Bitboard pieces(Color c, PieceType pt) const { return byColor[c] & byType[pt]; }
  Bitboard occupied() const { return byColor[WHITE] | byColor[BLACK]; }
  Bitboard empty() const { return ~occupied(); }

  // Position state
  Color sideToMove() const { return stm; }
  int castlingRights() const { return castling; }
  Square enPassantSquare() const { return epSquare; }
  int halfmoveClock() const { return halfmoves; }
  int fullmoveNumber() const { return fullmoves; }
  HashKey hash() const { return positionHash; }

  // Move history entry (public for search access)
  struct StateInfo {
    Move move;
    Piece captured;
    int castling;
    Square epSquare;
    int halfmoves;
    HashKey hash;
  };
  const std::vector<StateInfo>& getHistory() const { return history; }

  // Check detection
  bool inCheck() const;
  bool isCheckmate();
  bool isStalemate();
  bool isDraw() const;

  // Draw detection helpers
  bool isThreefoldRepetition() const;
  bool isInsufficientMaterial() const;
  int repetitionCount() const;

  // Attacks and pins
  Bitboard attacksTo(Square sq) const;
  Bitboard attacksFrom(Square sq) const;
  bool isAttacked(Square sq, Color attackerColor) const;

  // X-ray attacks (attacks through pieces)
  Bitboard xrayRookAttacks(Square sq, Bitboard blockers, Bitboard occ) const;
  Bitboard xrayBishopAttacks(Square sq, Bitboard blockers, Bitboard occ) const;

  // Pin detection
  Bitboard pinnedPieces(Color c) const;
  bool isPinned(Square sq, Color c) const;

  // Static Exchange Evaluation
  int see(Move move) const;

  // Position evaluation helpers (incrementally maintained)
  int materialCount(Color c) const { return material[c]; }
  int getMgPST() const { return mgPST; }          // White - Black non-king MG PST
  int getMgKingPST() const { return mgKingPST; }  // White - Black king MG PST
  int getEgKingPST() const { return egKingPST; }  // White - Black king EG PST
  int getGamePhase() const;                       // Tapered phase (0-256)

  // Print board
  void print() const;

 private:
  // Bitboards by piece type and color
  Bitboard byType[6];   // PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
  Bitboard byColor[2];  // WHITE, BLACK

  // Piece lookup table (for fast pieceAt queries)
  Piece board[64];

  // Position state
  Color stm;             // Side to move
  int castling;          // Castling rights
  Square epSquare;       // En passant square
  int halfmoves;         // Halfmove clock (for 50-move rule)
  int fullmoves;         // Fullmove number
  HashKey positionHash;  // Zobrist hash of position

  // Incrementally maintained evaluation accumulators
  int material[2];  // Material count per color (centipawns)
  int mgPST;        // White - Black non-king middlegame PST score
  int mgKingPST;    // White - Black king middlegame PST score
  int egKingPST;    // White - Black king endgame PST score
  int phase;        // Raw game phase (0-24, higher = more material)

  // Move history for unmake (StateInfo defined in public section)
  std::vector<StateInfo> history;

  // Helper methods
  void putPiece(Piece pc, Square sq);
  void removePiece(Square sq);
  void movePiece(Square from, Square to);
  void clear();

  // Update piece arrays
  void updateArrays();
};

// Starting position FEN
constexpr const char* STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
