#include "Bitboard.h"
#include "Magic.h"
#include "Polyglot.h"
#include "PolyglotTestUtils.h"
#include "Position.h"
#include "Types.h"
#include "Zobrist.h"

#include <cstdint>

#include <gtest/gtest.h>

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

TEST_F(PolyglotTest, Load_TruncatedFileFails) {
  testutil::ScopedTempBookFile bookFile("polyglot_truncated");
  ASSERT_TRUE(
      testutil::writeRawBytes(bookFile.path(), {0x46, 0x3B, 0x96, 0x18, 0x16, 0x91, 0xFC, 0x9C}));

  PolyglotBook book;
  EXPECT_FALSE(book.load(bookFile.path().string()));
  EXPECT_FALSE(book.isLoaded());
  EXPECT_EQ(book.size(), 0u);
}

TEST_F(PolyglotTest, Probe_ReturnsValidBookMove) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  testutil::ScopedTempBookFile bookFile("polyglot_valid_probe");
  const uint64_t key = PolyglotBook::computeHash(pos);
  ASSERT_TRUE(testutil::writePolyglotBook(bookFile.path(),
                                          {{key, testutil::encodePolyglotMove("e2e4"), 20, 0}}));

  PolyglotBook book;
  ASSERT_TRUE(book.load(bookFile.path().string()));

  EXPECT_EQ(book.probe(pos), testutil::findLegalMove(pos, "e2e4"));
}

TEST_F(PolyglotTest, GetMoves_ReturnsDuplicateKeyRangeSortedByWeight) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  Position otherPos;
  otherPos.setFromFEN("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2");

  testutil::ScopedTempBookFile bookFile("polyglot_duplicate_range");
  const uint64_t startKey = PolyglotBook::computeHash(pos);
  const uint64_t otherKey = PolyglotBook::computeHash(otherPos);
  ASSERT_TRUE(testutil::writePolyglotBook(
      bookFile.path(), {
                           {otherKey, testutil::encodePolyglotMove("g1f3"), 7, 0},
                           {startKey, testutil::encodePolyglotMove("e2e4"), 10, 0},
                           {startKey, testutil::encodePolyglotMove("d2d4"), 40, 0},
                       }));

  PolyglotBook book;
  ASSERT_TRUE(book.load(bookFile.path().string()));

  auto moves = book.getMoves(pos);
  ASSERT_EQ(moves.size(), 2u);
  EXPECT_EQ(moves[0].first, testutil::findLegalMove(pos, "d2d4"));
  EXPECT_EQ(moves[0].second, 40);
  EXPECT_EQ(moves[1].first, testutil::findLegalMove(pos, "e2e4"));
  EXPECT_EQ(moves[1].second, 10);
}

TEST_F(PolyglotTest, Probe_IgnoresIllegalBookMove) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  testutil::ScopedTempBookFile bookFile("polyglot_illegal_move");
  const uint64_t key = PolyglotBook::computeHash(pos);
  ASSERT_TRUE(testutil::writePolyglotBook(bookFile.path(),
                                          {
                                              {key, testutil::encodePolyglotMove("e2e5"), 100, 0},
                                              {key, testutil::encodePolyglotMove("d2d4"), 10, 0},
                                          }));

  PolyglotBook book;
  ASSERT_TRUE(book.load(bookFile.path().string()));

  auto moves = book.getMoves(pos);
  ASSERT_EQ(moves.size(), 1u);
  EXPECT_EQ(moves[0].first, testutil::findLegalMove(pos, "d2d4"));
  EXPECT_EQ(book.probe(pos), testutil::findLegalMove(pos, "d2d4"));
}

TEST_F(PolyglotTest, Probe_ZeroWeightMovesReturnNoMove) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  testutil::ScopedTempBookFile bookFile("polyglot_zero_weight");
  const uint64_t key = PolyglotBook::computeHash(pos);
  ASSERT_TRUE(testutil::writePolyglotBook(bookFile.path(),
                                          {{key, testutil::encodePolyglotMove("e2e4"), 0, 0}}));

  PolyglotBook book;
  ASSERT_TRUE(book.load(bookFile.path().string()));

  EXPECT_EQ(book.probe(pos), 0u);
}

TEST_F(PolyglotTest, Probe_RepeatedlyReturnsOnlyLoadedBookMoves) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  const Move e2e4 = testutil::findLegalMove(pos, "e2e4");
  const Move d2d4 = testutil::findLegalMove(pos, "d2d4");
  ASSERT_NE(e2e4, 0u);
  ASSERT_NE(d2d4, 0u);

  testutil::ScopedTempBookFile bookFile("polyglot_repeated_probe");
  const uint64_t key = PolyglotBook::computeHash(pos);
  ASSERT_TRUE(testutil::writePolyglotBook(bookFile.path(),
                                          {
                                              {key, testutil::encodePolyglotMove("e2e4"), 30, 0},
                                              {key, testutil::encodePolyglotMove("d2d4"), 20, 0},
                                          }));

  PolyglotBook book;
  ASSERT_TRUE(book.load(bookFile.path().string()));

  for (int i = 0; i < 500; ++i) {
    const Move move = book.probe(pos);
    EXPECT_TRUE(move == e2e4 || move == d2d4) << "Unexpected move at iteration " << i;
  }
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
