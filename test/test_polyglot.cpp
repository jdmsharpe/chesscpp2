#include <gtest/gtest.h>

#include "Bitboard.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Polyglot.h"
#include "Position.h"
#include "Zobrist.h"

class PolyglotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

// =============================================================================
// Polyglot Hash Tests - Known values from specification
// =============================================================================

// Source: http://hgm.nubati.net/book_format.html
TEST_F(PolyglotTest, Hash_StartingPosition) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  uint64_t hash = PolyglotBook::computeHash(pos);
  EXPECT_EQ(hash, 0x463b96181691fc9cULL);
}

// Test with a position that has en passant capture possible
TEST_F(PolyglotTest, Hash_WithEnPassantCapture) {
  // Position after 1.a4 b5 2.h4 b4 3.c4 - black pawn on b4 can capture on c3
  Position pos;
  pos.setFromFEN("rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3");
  uint64_t hash = PolyglotBook::computeHash(pos);
  EXPECT_EQ(hash, 0x3c8123ea7b067637ULL);
}

// Position after en passant capture and Ra3
TEST_F(PolyglotTest, Hash_AfterEnPassantCapture) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/p1pppppp/8/8/P6P/R1p5/1P1PPPP1/1NBQKBNR b Kkq - 0 4");
  uint64_t hash = PolyglotBook::computeHash(pos);
  EXPECT_EQ(hash, 0x5c3f9b829b279560ULL);
}

// =============================================================================
// Book Loading Tests
// =============================================================================

TEST_F(PolyglotTest, Load_NonexistentFile) {
  PolyglotBook book;
  EXPECT_FALSE(book.load("/nonexistent/path/book.bin"));
  EXPECT_FALSE(book.isLoaded());
  EXPECT_EQ(book.size(), 0u);
}

TEST_F(PolyglotTest, Probe_UnloadedBook) {
  PolyglotBook book;
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  // Should return 0 (no move) when book is not loaded
  Move move = book.probe(pos);
  EXPECT_EQ(move, 0u);
}

// =============================================================================
// Hash Consistency Tests
// =============================================================================

TEST_F(PolyglotTest, Hash_SamePositionSameHash) {
  Position pos1, pos2;
  pos1.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
  pos2.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");

  EXPECT_EQ(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

TEST_F(PolyglotTest, Hash_DifferentPositions) {
  Position pos1, pos2;
  pos1.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  pos2.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");

  EXPECT_NE(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

TEST_F(PolyglotTest, Hash_CastlingRightsMatter) {
  Position pos1, pos2;
  // Same position, different castling rights
  pos1.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  pos2.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w - - 0 1");

  EXPECT_NE(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

TEST_F(PolyglotTest, Hash_SideToMoveMatter) {
  Position pos1, pos2;
  // Same position, different side to move
  pos1.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  pos2.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");

  EXPECT_NE(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

TEST_F(PolyglotTest, Hash_EnPassantMatters_WhenCaptureIsPossible) {
  // Position where en passant IS possible: black pawn on b4 can capture on c3
  Position pos1;
  pos1.setFromFEN("rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3");

  // Same position without en passant
  Position pos2;
  pos2.setFromFEN("rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq - 0 3");

  // When en passant capture is possible, the hashes should differ
  EXPECT_NE(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

TEST_F(PolyglotTest, Hash_EnPassantIgnored_WhenNoCaptureIsPossible) {
  // Position after 1.e4 - en passant at e3 but NO black pawn on d4 or f4
  Position pos1;
  pos1.setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

  Position pos2;
  pos2.setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");

  // When no en passant capture is possible, Polyglot spec says the hashes should be equal
  EXPECT_EQ(PolyglotBook::computeHash(pos1), PolyglotBook::computeHash(pos2));
}

// =============================================================================
// Move Conversion Tests
// =============================================================================

TEST_F(PolyglotTest, GetMoves_UnloadedBook) {
  PolyglotBook book;
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  auto moves = book.getMoves(pos);
  EXPECT_TRUE(moves.empty());
}
