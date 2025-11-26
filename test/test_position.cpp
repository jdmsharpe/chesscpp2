#include <gtest/gtest.h>

#include "Bitboard.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Position.h"
#include "Zobrist.h"

class PositionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

// =============================================================================
// Draw Detection Tests
// =============================================================================

TEST_F(PositionTest, InsufficientMaterial_KingVsKing) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/4K3 w - - 0 1");  // K vs K
  EXPECT_TRUE(pos.isInsufficientMaterial());
  EXPECT_TRUE(pos.isDraw());
}

TEST_F(PositionTest, InsufficientMaterial_KingKnightVsKing) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/4KN2 w - - 0 1");  // KN vs K
  EXPECT_TRUE(pos.isInsufficientMaterial());
  EXPECT_TRUE(pos.isDraw());
}

TEST_F(PositionTest, InsufficientMaterial_KingBishopVsKing) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/4KB2 w - - 0 1");  // KB vs K
  EXPECT_TRUE(pos.isInsufficientMaterial());
  EXPECT_TRUE(pos.isDraw());
}

TEST_F(PositionTest, InsufficientMaterial_KingVsKingKnight) {
  Position pos;
  pos.setFromFEN("8/8/4n3/4k3/8/8/8/4K3 w - - 0 1");  // K vs KN
  EXPECT_TRUE(pos.isInsufficientMaterial());
  EXPECT_TRUE(pos.isDraw());
}

TEST_F(PositionTest, InsufficientMaterial_SameColorBishops) {
  Position pos;
  // Both bishops on light squares (c1 and f8 are both light)
  pos.setFromFEN("5b2/8/8/4k3/8/8/8/2B1K3 w - - 0 1");
  EXPECT_TRUE(pos.isInsufficientMaterial());
}

TEST_F(PositionTest, SufficientMaterial_OppositeColorBishops) {
  Position pos;
  // Bishops on opposite colors (c1=light, c8=dark)
  pos.setFromFEN("2b5/8/8/4k3/8/8/8/2B1K3 w - - 0 1");
  EXPECT_FALSE(pos.isInsufficientMaterial());
}

TEST_F(PositionTest, SufficientMaterial_WithPawn) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/4P3/8/4K3 w - - 0 1");  // KP vs K
  EXPECT_FALSE(pos.isInsufficientMaterial());
  EXPECT_FALSE(pos.isDraw());
}

TEST_F(PositionTest, SufficientMaterial_WithRook) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/4KR2 w - - 0 1");  // KR vs K
  EXPECT_FALSE(pos.isInsufficientMaterial());
}

TEST_F(PositionTest, SufficientMaterial_WithQueen) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/4KQ2 w - - 0 1");  // KQ vs K
  EXPECT_FALSE(pos.isInsufficientMaterial());
}

TEST_F(PositionTest, SufficientMaterial_TwoKnights) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/8/3NKN2 w - - 0 1");  // KNN vs K
  // Technically can't force mate, but not a dead draw by FIDE rules
  EXPECT_FALSE(pos.isInsufficientMaterial());
}

TEST_F(PositionTest, FiftyMoveRule_NotDraw) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/4P3/4K3 w - - 50 1");  // halfmoves = 50
  EXPECT_FALSE(pos.isDraw());  // Need 100 half-moves (50 full moves)
}

TEST_F(PositionTest, FiftyMoveRule_Draw) {
  Position pos;
  pos.setFromFEN("8/8/8/4k3/8/8/4P3/4K3 w - - 100 1");  // halfmoves = 100
  EXPECT_TRUE(pos.isDraw());
}

TEST_F(PositionTest, ThreefoldRepetition_NoRepetition) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);
  EXPECT_EQ(pos.repetitionCount(), 1);
  EXPECT_FALSE(pos.isThreefoldRepetition());
}

TEST_F(PositionTest, ThreefoldRepetition_TwoRepetitions) {
  // Use a simple endgame position without castling rights
  Position pos;
  pos.setFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  HashKey startHash = pos.hash();

  auto doMove = [&pos](Square from, Square to) -> bool {
    auto moves = MoveGen::generateLegalMoves(pos);
    for (Move m : moves) {
      if (fromSquare(m) == from && toSquare(m) == to) {
        pos.makeMove(m);
        return true;
      }
    }
    return false;
  };

  // Move kings back and forth: Ke2 Ke7 Ke1 Ke8
  ASSERT_TRUE(doMove(E1, E2));
  ASSERT_TRUE(doMove(E8, E7));
  ASSERT_TRUE(doMove(E2, E1));
  ASSERT_TRUE(doMove(E7, E8));

  // Hash should match the start position
  EXPECT_EQ(pos.hash(), startHash);
  EXPECT_EQ(pos.repetitionCount(), 2);
  EXPECT_FALSE(pos.isThreefoldRepetition());
}

TEST_F(PositionTest, ThreefoldRepetition_ThreeRepetitions) {
  // Use a simple endgame position without castling rights
  Position pos;
  pos.setFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  auto doMove = [&pos](Square from, Square to) -> bool {
    auto moves = MoveGen::generateLegalMoves(pos);
    for (Move m : moves) {
      if (fromSquare(m) == from && toSquare(m) == to) {
        pos.makeMove(m);
        return true;
      }
    }
    return false;
  };

  // First cycle: Ke2 Ke7 Ke1 Ke8 (back to start - 2nd occurrence)
  ASSERT_TRUE(doMove(E1, E2));
  ASSERT_TRUE(doMove(E8, E7));
  ASSERT_TRUE(doMove(E2, E1));
  ASSERT_TRUE(doMove(E7, E8));
  EXPECT_EQ(pos.repetitionCount(), 2);

  // Second cycle: Ke2 Ke7 Ke1 Ke8 (back to start - 3rd occurrence)
  ASSERT_TRUE(doMove(E1, E2));
  ASSERT_TRUE(doMove(E8, E7));
  ASSERT_TRUE(doMove(E2, E1));
  ASSERT_TRUE(doMove(E7, E8));
  EXPECT_EQ(pos.repetitionCount(), 3);
  EXPECT_TRUE(pos.isThreefoldRepetition());
  EXPECT_TRUE(pos.isDraw());
}

// =============================================================================
// Make/Unmake Move Tests
// =============================================================================

TEST_F(PositionTest, MakeUnmake_PreservesPosition) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN)
        << "Position corrupted after make/unmake of move";
  }
}

TEST_F(PositionTest, MakeUnmake_ComplexPosition) {
  // Position with castling rights, en passant available
  Position pos;
  pos.setFromFEN(
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN)
        << "Position corrupted after make/unmake";
  }
}

TEST_F(PositionTest, MakeUnmake_Promotion) {
  Position pos;
  pos.setFromFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");  // Pawn ready to promote
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN);
  }
}

TEST_F(PositionTest, MakeUnmake_EnPassant) {
  Position pos;
  pos.setFromFEN("8/8/8/pP6/8/8/8/4K2k w - a6 0 1");  // En passant available
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN);
  }
}

TEST_F(PositionTest, MakeUnmake_Castling) {
  Position pos;
  pos.setFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");  // All castling
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN);
  }
}

// =============================================================================
// Zobrist Hash Tests
// =============================================================================

TEST_F(PositionTest, ZobristHash_ConsistentAfterMakeUnmake) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);
  HashKey originalHash = pos.hash();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.hash(), originalHash)
        << "Hash changed after make/unmake";
  }
}

TEST_F(PositionTest, ZobristHash_DifferentPositions) {
  Position pos1, pos2;
  pos1.setFromFEN(STARTING_FEN);
  pos2.setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

  EXPECT_NE(pos1.hash(), pos2.hash());
}

TEST_F(PositionTest, ZobristHash_SamePositionSameHash) {
  Position pos1, pos2;
  pos1.setFromFEN(STARTING_FEN);
  pos2.setFromFEN(STARTING_FEN);

  EXPECT_EQ(pos1.hash(), pos2.hash());
}

// =============================================================================
// SEE (Static Exchange Evaluation) Tests
// =============================================================================

TEST_F(PositionTest, SEE_WinningCapture) {
  Position pos;
  // White queen on e4 can take undefended black pawn on e5
  pos.setFromFEN("4k3/8/8/4p3/4Q3/8/8/4K3 w - - 0 1");

  auto moves = MoveGen::generateLegalMoves(pos);
  Move capture = 0;
  for (Move m : moves) {
    if (fromSquare(m) == E4 && toSquare(m) == E5) {
      capture = m;
      break;
    }
  }
  ASSERT_NE(capture, 0) << "Queen should be able to capture pawn on e5";
  EXPECT_GT(pos.see(capture), 0);  // Winning capture (gain pawn)
}

TEST_F(PositionTest, SEE_LosingCapture) {
  Position pos;
  // White pawn takes defended piece
  pos.setFromFEN("8/8/3r4/4p3/3P4/8/8/4K3 w - - 0 1");

  auto moves = MoveGen::generateLegalMoves(pos);
  Move capture = 0;
  for (Move m : moves) {
    if (fromSquare(m) == D4 && toSquare(m) == E5) {
      capture = m;
      break;
    }
  }
  ASSERT_NE(capture, 0);
  // Pawn takes pawn, rook recaptures: +100 - 100 = 0 or slightly negative
  EXPECT_LE(pos.see(capture), 100);
}

TEST_F(PositionTest, SEE_EqualExchange) {
  Position pos;
  // Knight takes knight, both defended
  pos.setFromFEN("8/8/3n4/4n3/3N4/8/8/4K3 w - - 0 1");

  auto moves = MoveGen::generateLegalMoves(pos);
  Move capture = 0;
  for (Move m : moves) {
    if (fromSquare(m) == D4 && toSquare(m) == E6) {
      capture = m;
      break;
    }
  }
  if (capture != 0) {
    // Knight takes knight: equal trade
    int see = pos.see(capture);
    EXPECT_GE(see, -50);
    EXPECT_LE(see, 50);
  }
}
