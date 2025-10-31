#include <gtest/gtest.h>

#include "Bitboard.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Position.h"
#include "Zobrist.h"

class MoveGenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

TEST_F(MoveGenTest, StartingPosition) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);

  // Test piece counts
  EXPECT_EQ(BB::popCount(pos.pieces(WHITE, PAWN)), 8);
  EXPECT_EQ(BB::popCount(pos.pieces(BLACK, PAWN)), 8);
  EXPECT_EQ(BB::popCount(pos.pieces(WHITE, KNIGHT)), 2);
  EXPECT_EQ(BB::popCount(pos.pieces(BLACK, KNIGHT)), 2);

  // Test legal moves from starting position
  std::vector<Move> legalMoves = MoveGen::generateLegalMoves(pos);
  EXPECT_EQ(legalMoves.size(), 20);  // 16 pawn moves + 4 knight moves
}

TEST_F(MoveGenTest, PerftStartingPosition) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);

  EXPECT_EQ(MoveGen::perft(pos, 1), 20);
  EXPECT_EQ(MoveGen::perft(pos, 2), 400);
  EXPECT_EQ(MoveGen::perft(pos, 3), 8902);
  EXPECT_EQ(MoveGen::perft(pos, 4), 197281);
}

TEST_F(MoveGenTest, PerftKiwipete) {
  Position pos;
  pos.setFromFEN(
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

  EXPECT_EQ(MoveGen::perft(pos, 1), 48);
  EXPECT_EQ(MoveGen::perft(pos, 2), 2039);
}

TEST_F(MoveGenTest, PerftCastlingRights) {
  Position pos;
  pos.setFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

  EXPECT_EQ(MoveGen::perft(pos, 1), 14);
}

TEST_F(MoveGenTest, PerftPromotions) {
  Position pos;
  pos.setFromFEN(
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");

  EXPECT_EQ(MoveGen::perft(pos, 1), 6);
}

TEST_F(MoveGenTest, PerftMiddleGame) {
  Position pos;
  pos.setFromFEN("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");

  EXPECT_EQ(MoveGen::perft(pos, 1), 44);
}

TEST_F(MoveGenTest, PerftEndgame) {
  Position pos;
  pos.setFromFEN(
      "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 "
      "10");

  EXPECT_EQ(MoveGen::perft(pos, 1), 46);
}

TEST_F(MoveGenTest, FENLoading) {
  Position pos;

  // Test starting position
  pos.setFromFEN(STARTING_FEN);
  std::string fen = pos.getFEN();
  EXPECT_EQ(fen, STARTING_FEN);

  // Test another position
  std::string testFEN =
      "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  pos.setFromFEN(testFEN);
  fen = pos.getFEN();
  EXPECT_EQ(fen, testFEN);
}

TEST_F(MoveGenTest, CheckDetection) {
  Position pos;

  // Position with white in check
  pos.setFromFEN(
      "rnb1kbnr/pppp1ppp/4p3/8/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
  EXPECT_TRUE(pos.inCheck());

  // Position without check
  pos.setFromFEN(STARTING_FEN);
  EXPECT_FALSE(pos.inCheck());
}

TEST_F(MoveGenTest, MagicBitboardsRook) {
  // Test rook attacks from E4 with no blockers
  EXPECT_EQ(BB::popCount(Magic::rookAttacks(E4, 0)), 14);

  // Test rook attacks from E4 with blocker on E6
  Bitboard blockers = BB::squareBB(E6);
  Bitboard rookAttacks = Magic::rookAttacks(E4, blockers);
  EXPECT_TRUE(BB::testBit(rookAttacks, E5));
  EXPECT_TRUE(BB::testBit(rookAttacks, E6));
  EXPECT_FALSE(BB::testBit(rookAttacks, E7));  // Blocked
}

TEST_F(MoveGenTest, MagicBitboardsBishop) {
  // Test bishop attacks
  EXPECT_EQ(BB::popCount(Magic::bishopAttacks(E4, 0)), 13);
}

TEST_F(MoveGenTest, MagicBitboardsQueen) {
  // Test queen attacks (combination)
  EXPECT_EQ(BB::popCount(Magic::queenAttacks(E4, 0)), 27);
}

TEST_F(MoveGenTest, DiagonalWrappingBugFix) {
  // Test diagonal wrapping bug fix - bishop on F8 should NOT attack H1
  Position pos;
  pos.setFromFEN(
      "rnbqkbnr/pppp1ppp/4p3/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
  Bitboard occupied = pos.occupied();
  Bitboard bishopAttacks = Magic::bishopAttacks(F8, occupied);
  EXPECT_FALSE(
      BB::testBit(bishopAttacks, H1));  // Should NOT attack h1 (wrapping bug)
  EXPECT_TRUE(BB::testBit(bishopAttacks, G7));  // Should attack g7
}
