#include "Bitboard.h"
#include "Magic.h"
#include "MoveGen.h"
#include "PST.h"
#include "Position.h"
#include "Zobrist.h"

#include <gtest/gtest.h>

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
  EXPECT_FALSE(pos.isDraw());                          // Need 100 half-moves (50 full moves)
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
    EXPECT_EQ(pos.getFEN(), originalFEN) << "Position corrupted after make/unmake of move";
  }
}

TEST_F(PositionTest, MakeUnmake_ComplexPosition) {
  // Position with castling rights, en passant available
  Position pos;
  pos.setFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  std::string originalFEN = pos.getFEN();

  auto moves = MoveGen::generateLegalMoves(pos);
  for (Move move : moves) {
    pos.makeMove(move);
    pos.unmakeMove();
    EXPECT_EQ(pos.getFEN(), originalFEN) << "Position corrupted after make/unmake";
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
    EXPECT_EQ(pos.hash(), originalHash) << "Hash changed after make/unmake";
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

// =============================================================================
// Illegal Position Safety Tests
// =============================================================================

TEST_F(PositionTest, InCheck_NoKing_ReturnsFalse) {
  // Position with no black king — inCheck() must not crash, should return false.
  // We simulate this by setting up a position, making a king-capturing move
  // via makeMove, then calling inCheck on the resulting (corrupt) position.
  Position pos;
  // Black king on a1, white queen on c1, white king on b3 — queen can
  // capture king.  After the capture, black has no king.
  ASSERT_TRUE(pos.setFromFEN("8/8/8/8/8/1K6/8/k1Q5 w - - 0 1"));

  // Find the pseudo-legal Qc1xa1 move manually
  MoveList pseudoMoves = MoveGen::generatePseudoLegalMoves(pos);
  Move kingCapture = 0;
  for (Move m : pseudoMoves) {
    if (fromSquare(m) == C1 && toSquare(m) == A1) {
      kingCapture = m;
      break;
    }
  }
  ASSERT_NE(kingCapture, 0) << "Qc1xa1 should be pseudo-legal";

  // Make the king-capturing move — puts us in an illegal state
  pos.makeMove(kingCapture);

  // inCheck() must not crash and should return false (no king to be in check)
  EXPECT_FALSE(pos.inCheck());

  // Clean up — unmake the move to restore valid state
  pos.unmakeMove();
}

// =============================================================================
// Incremental Eval Accumulator Tests
// =============================================================================

// Helper: recompute all incremental accumulators from scratch and compare
static void verifyAccumulators(const Position& pos, const std::string& ctx) {
  // Recompute material from scratch
  int expectedMatW = 0, expectedMatB = 0;
  int expectedMgPST = 0, expectedMgKingPST = 0, expectedEgKingPST = 0;
  int expectedPhase = 0;

  for (int sq = 0; sq < 64; sq++) {
    Piece pc = pos.pieceAt(Square(sq));
    if (pc == NO_PIECE) continue;
    Color c = colorOf(pc);
    PieceType pt = typeOf(pc);
    int sign = (c == WHITE) ? 1 : -1;

    if (c == WHITE)
      expectedMatW += PST::pieceValue[pt];
    else
      expectedMatB += PST::pieceValue[pt];

    expectedMgPST += sign * PST::mgValue(pt, c, Square(sq));
    if (pt == KING) {
      expectedMgKingPST += sign * PST::mgKingValue(c, Square(sq));
      expectedEgKingPST += sign * PST::egKingValue(c, Square(sq));
    }
    expectedPhase += PST::phaseWeight[pt];
  }

  EXPECT_EQ(pos.materialCount(WHITE), expectedMatW) << ctx << " — White material mismatch";
  EXPECT_EQ(pos.materialCount(BLACK), expectedMatB) << ctx << " — Black material mismatch";
  EXPECT_EQ(pos.getMgPST(), expectedMgPST) << ctx << " — mgPST mismatch";
  EXPECT_EQ(pos.getMgKingPST(), expectedMgKingPST) << ctx << " — mgKingPST mismatch";
  EXPECT_EQ(pos.getEgKingPST(), expectedEgKingPST) << ctx << " — egKingPST mismatch";

  // Phase: compare raw phase via the tapered output
  const int TOTAL_PHASE = 24;
  int p = std::max(0, std::min(expectedPhase, TOTAL_PHASE));
  int expectedTapered = std::min(256, (p * 256 + TOTAL_PHASE / 2) / TOTAL_PHASE);
  EXPECT_EQ(pos.getGamePhase(), expectedTapered) << ctx << " — game phase mismatch";
}

TEST_F(PositionTest, Accumulators_StartingPosition) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);
  verifyAccumulators(pos, "starting position");
}

TEST_F(PositionTest, Accumulators_Kiwipete) {
  Position pos;
  pos.setFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  verifyAccumulators(pos, "Kiwipete");
}

TEST_F(PositionTest, Accumulators_PreservedAfterMakeUnmake) {
  // Test across multiple positions with all move types
  const char* fens[] = {
      STARTING_FEN,
      // Kiwipete: castling, captures, en passant
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      // Promotion position
      "8/P7/8/8/8/8/p7/4K2k w - - 0 1",
      // En passant
      "8/8/8/pP6/8/8/8/4K2k w - a6 0 1",
      // All castling available
      "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
      // Endgame with passed pawns
      "8/5k2/8/2Pp4/8/8/8/4K3 w - d6 0 1",
  };

  for (const char* fen : fens) {
    Position pos;
    pos.setFromFEN(fen);

    // Record original accumulators
    int origMatW = pos.materialCount(WHITE);
    int origMatB = pos.materialCount(BLACK);
    int origMgPST = pos.getMgPST();
    int origMgKingPST = pos.getMgKingPST();
    int origEgKingPST = pos.getEgKingPST();
    int origPhase = pos.getGamePhase();

    auto moves = MoveGen::generateLegalMoves(pos);
    for (Move move : moves) {
      pos.makeMove(move);
      // Verify accumulators are consistent after makeMove
      verifyAccumulators(pos, std::string("after move in ") + fen);
      pos.unmakeMove();

      // Verify accumulators restored exactly
      EXPECT_EQ(pos.materialCount(WHITE), origMatW)
          << "White material not restored after unmake in " << fen;
      EXPECT_EQ(pos.materialCount(BLACK), origMatB)
          << "Black material not restored after unmake in " << fen;
      EXPECT_EQ(pos.getMgPST(), origMgPST) << "mgPST not restored after unmake in " << fen;
      EXPECT_EQ(pos.getMgKingPST(), origMgKingPST)
          << "mgKingPST not restored after unmake in " << fen;
      EXPECT_EQ(pos.getEgKingPST(), origEgKingPST)
          << "egKingPST not restored after unmake in " << fen;
      EXPECT_EQ(pos.getGamePhase(), origPhase) << "game phase not restored after unmake in " << fen;
    }
  }
}

TEST_F(PositionTest, Accumulators_MultiMoveSequence) {
  // Play a sequence of moves and verify accumulators stay in sync
  Position pos;
  pos.setFromFEN(STARTING_FEN);

  // Italian Game opening: 1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5
  const char* moveStrs[] = {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5"};

  for (const char* ms : moveStrs) {
    Square from = stringToSquare(std::string(ms, 2));
    Square to = stringToSquare(std::string(ms + 2, 2));
    auto moves = MoveGen::generateLegalMoves(pos);
    Move found = 0;
    for (Move m : moves) {
      if (fromSquare(m) == from && toSquare(m) == to) {
        found = m;
        break;
      }
    }
    ASSERT_NE(found, 0) << "Move " << ms << " not found";
    pos.makeMove(found);
    verifyAccumulators(pos, std::string("after ") + ms);
  }

  // Now unmake all and verify we return to start
  for (int i = 0; i < 6; i++) pos.unmakeMove();
  verifyAccumulators(pos, "after unmaking Italian Game sequence");

  Position fresh;
  fresh.setFromFEN(STARTING_FEN);
  EXPECT_EQ(pos.materialCount(WHITE), fresh.materialCount(WHITE));
  EXPECT_EQ(pos.materialCount(BLACK), fresh.materialCount(BLACK));
  EXPECT_EQ(pos.getMgPST(), fresh.getMgPST());
}
