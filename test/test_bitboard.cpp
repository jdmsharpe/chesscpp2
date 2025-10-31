#include <gtest/gtest.h>

#include "Bitboard.h"

class BitboardTest : public ::testing::Test {
 protected:
  void SetUp() override { BB::init(); }
};

TEST_F(BitboardTest, BasicOperations) {
  // Test square bit operations
  Bitboard bb = BB::squareBB(E4);
  EXPECT_TRUE(BB::testBit(bb, E4));
  EXPECT_FALSE(BB::testBit(bb, E5));

  // Test set/clear
  bb = BB::setBit(bb, E5);
  EXPECT_TRUE(BB::testBit(bb, E5));
  bb = BB::clearBit(bb, E4);
  EXPECT_FALSE(BB::testBit(bb, E4));

  // Test popcount
  bb = BB::squareBB(A1) | BB::squareBB(H8) | BB::squareBB(E4);
  EXPECT_EQ(BB::popCount(bb), 3);
}

TEST_F(BitboardTest, PawnPushes) {
  // Test white pawn pushes
  EXPECT_EQ(BB::pawnPush<WHITE>(BB::squareBB(E2)), BB::squareBB(E3));

  // Test black pawn pushes
  EXPECT_EQ(BB::pawnPush<BLACK>(BB::squareBB(E7)), BB::squareBB(E6));
}

TEST_F(BitboardTest, PawnAttacks) {
  // Test pawn attacks
  Bitboard attacks = BB::pawnAttacks<WHITE>(BB::squareBB(E4));
  EXPECT_TRUE(BB::testBit(attacks, D5));
  EXPECT_TRUE(BB::testBit(attacks, F5));
  EXPECT_EQ(BB::popCount(attacks), 2);
}

TEST_F(BitboardTest, KnightAttacks) {
  // Test knight attacks from E4
  Bitboard attacks = BB::knightAttacks(E4);
  EXPECT_EQ(BB::popCount(attacks), 8);  // Knight on E4 has 8 moves
  EXPECT_TRUE(BB::testBit(attacks, D2));
  EXPECT_TRUE(BB::testBit(attacks, F2));
  EXPECT_TRUE(BB::testBit(attacks, C3));

  // Test knight attacks from corner
  EXPECT_EQ(BB::popCount(BB::knightAttacks(A1)),
            2);  // Knight on A1 has 2 moves
}

TEST_F(BitboardTest, KingAttacks) {
  // Test king attacks from center
  EXPECT_EQ(BB::popCount(BB::kingAttacks(E4)), 8);  // King on E4 has 8 moves

  // Test king attacks from corner
  EXPECT_EQ(BB::popCount(BB::kingAttacks(A1)), 3);  // King on A1 has 3 moves
}

TEST_F(BitboardTest, FilesAndRanks) {
  // Test file operations
  EXPECT_EQ(fileOf(E4), FILE_E);
  EXPECT_EQ(rankOf(E4), RANK_4);

  // Test file bitboards
  Bitboard fileE = BB::fileBB(E4);
  EXPECT_EQ(BB::popCount(fileE), 8);
  EXPECT_TRUE(BB::testBit(fileE, E1));
  EXPECT_TRUE(BB::testBit(fileE, E8));

  // Test rank bitboards
  Bitboard rank4 = BB::rankBB(E4);
  EXPECT_EQ(BB::popCount(rank4), 8);
  EXPECT_TRUE(BB::testBit(rank4, A4));
  EXPECT_TRUE(BB::testBit(rank4, H4));
}
