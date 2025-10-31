#pragma once

#include <cstdint>
#include <string>

// Bitboard type - represents 64 squares as bits
using Bitboard = uint64_t;

// Hash key for position
using HashKey = uint64_t;

// Square representation (0-63)
using Square = int;

// Piece types
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE };

// Colors
enum Color { WHITE, BLACK, NO_COLOR };

// Piece representation (combines color and type)
enum Piece {
  W_PAWN,
  W_KNIGHT,
  W_BISHOP,
  W_ROOK,
  W_QUEEN,
  W_KING,
  B_PAWN,
  B_KNIGHT,
  B_BISHOP,
  B_ROOK,
  B_QUEEN,
  B_KING,
  NO_PIECE
};

// Castling rights
enum CastlingRight {
  WHITE_OO = 1,   // White kingside
  WHITE_OOO = 2,  // White queenside
  BLACK_OO = 4,   // Black kingside
  BLACK_OOO = 8,  // Black queenside
  NO_CASTLING = 0,
  ALL_CASTLING = 15
};

// Move representation (16 bits)
// Bits 0-5: from square
// Bits 6-11: to square
// Bits 12-13: promotion piece type (0=knight, 1=bishop, 2=rook, 3=queen)
// Bits 14-15: special move flags
using Move = uint16_t;

// Move flags
constexpr Move NORMAL_MOVE = 0;
constexpr Move PROMOTION = 1 << 14;
constexpr Move EN_PASSANT = 2 << 14;
constexpr Move CASTLING = 3 << 14;

// Square constants
enum Squares {
  A1,
  B1,
  C1,
  D1,
  E1,
  F1,
  G1,
  H1,
  A2,
  B2,
  C2,
  D2,
  E2,
  F2,
  G2,
  H2,
  A3,
  B3,
  C3,
  D3,
  E3,
  F3,
  G3,
  H3,
  A4,
  B4,
  C4,
  D4,
  E4,
  F4,
  G4,
  H4,
  A5,
  B5,
  C5,
  D5,
  E5,
  F5,
  G5,
  H5,
  A6,
  B6,
  C6,
  D6,
  E6,
  F6,
  G6,
  H6,
  A7,
  B7,
  C7,
  D7,
  E7,
  F7,
  G7,
  H7,
  A8,
  B8,
  C8,
  D8,
  E8,
  F8,
  G8,
  H8,
  NO_SQUARE = 64
};

// Files and ranks
constexpr int FILE_A = 0, FILE_B = 1, FILE_C = 2, FILE_D = 3;
constexpr int FILE_E = 4, FILE_F = 5, FILE_G = 6, FILE_H = 7;
constexpr int RANK_1 = 0, RANK_2 = 1, RANK_3 = 2, RANK_4 = 3;
constexpr int RANK_5 = 4, RANK_6 = 5, RANK_7 = 6, RANK_8 = 7;

// Utility functions
constexpr Square makeSquare(int file, int rank) { return rank * 8 + file; }

constexpr int fileOf(Square sq) { return sq & 7; }

constexpr int rankOf(Square sq) { return sq >> 3; }

constexpr Color operator~(Color c) { return Color(c ^ 1); }

constexpr Bitboard fileBB(int file) { return 0x0101010101010101ULL << file; }

constexpr Bitboard rankBB(int rank) { return 0xFFULL << (rank * 8); }

// Move construction and extraction
constexpr Move makeMove(Square from, Square to) { return (to << 6) | from; }

constexpr Move makePromotion(Square from, Square to, PieceType promoPiece) {
  return PROMOTION | ((promoPiece - KNIGHT) << 12) | (to << 6) | from;
}

constexpr Move makeEnPassant(Square from, Square to) {
  return EN_PASSANT | (to << 6) | from;
}

constexpr Move makeCastling(Square from, Square to) {
  return CASTLING | (to << 6) | from;
}

constexpr Square fromSquare(Move m) { return m & 0x3F; }

constexpr Square toSquare(Move m) { return (m >> 6) & 0x3F; }

constexpr int moveType(Move m) { return m & (3 << 14); }

constexpr PieceType promotionType(Move m) {
  return PieceType(((m >> 12) & 3) + KNIGHT);
}

// String conversions
inline std::string squareToString(Square sq) {
  if (sq == NO_SQUARE) return "-";
  char file = 'a' + fileOf(sq);
  char rank = '1' + rankOf(sq);
  return std::string(1, file) + std::string(1, rank);
}

inline Square stringToSquare(const std::string& str) {
  if (str.length() != 2) return NO_SQUARE;
  int file = str[0] - 'a';
  int rank = str[1] - '1';
  if (file < 0 || file > 7 || rank < 0 || rank > 7) return NO_SQUARE;
  return makeSquare(file, rank);
}

inline std::string moveToString(Move m) {
  std::string result =
      squareToString(fromSquare(m)) + squareToString(toSquare(m));
  if (moveType(m) == PROMOTION) {
    const char* pieces = "nbrq";
    result += pieces[promotionType(m) - KNIGHT];
  }
  return result;
}

// Piece functions
constexpr Piece makePiece(Color c, PieceType pt) { return Piece(c * 6 + pt); }

constexpr Color colorOf(Piece pc) { return Color(pc / 6); }

constexpr PieceType typeOf(Piece pc) { return PieceType(pc % 6); }
