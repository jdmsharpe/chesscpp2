#pragma once

#include "Types.h"

// Shared piece-square tables and material values for incremental evaluation.
// Used by Position (incremental update) and Eval (full evaluation).

namespace PST {

// Material values (centipawns)
constexpr int pieceValue[6] = {100, 320, 330, 500, 900, 0};  // PAWN..KING

// Phase weights per piece type (total phase = 24)
constexpr int phaseWeight[6] = {0, 1, 1, 2, 4, 0};  // PAWN..KING

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

// Lookup PST value for a piece on a square (middlegame, non-king).
// Queens have no PST — their value comes from mobility evaluation.
// Returns value from White's perspective; for Black, flip the square (sq ^ 56)
constexpr const int* table[6] = {pawn, knight, bishop, rook, nullptr, nullptr};

// Get middlegame PST bonus for a non-king piece (from White's POV)
// King MG is tracked separately via mgKingValue() for tapering
inline int mgValue(PieceType pt, Color c, Square sq) {
    if (pt >= QUEEN) return 0;  // No PST for queen; king tracked separately
    Square oriented = (c == WHITE) ? sq : Square(sq ^ 56);
    return table[pt][oriented];
}

// Get middlegame king PST bonus (from White's POV)
inline int mgKingValue(Color c, Square sq) {
    Square oriented = (c == WHITE) ? sq : Square(sq ^ 56);
    return kingMiddle[oriented];
}

// Get endgame king PST bonus (from White's POV)
inline int egKingValue(Color c, Square sq) {
    Square oriented = (c == WHITE) ? sq : Square(sq ^ 56);
    return kingEndgame[oriented];
}

}  // namespace PST
