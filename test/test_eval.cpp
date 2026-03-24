#include <gtest/gtest.h>

#include "Bitboard.h"
#include "Eval.h"
#include "Magic.h"
#include "Position.h"
#include "Zobrist.h"

class EvalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

// =============================================================================
// Starting Position Balance
// =============================================================================

TEST_F(EvalTest, StartingPosition_NearZero) {
  Position pos;
  pos.setFromFEN(STARTING_FEN);
  int score = Eval::evaluate(pos);
  // Starting position should be roughly equal: within [-50, +50] cp
  EXPECT_GT(score, -50) << "Starting position too negative: " << score;
  EXPECT_LT(score, 50) << "Starting position too positive: " << score;
}

// =============================================================================
// Material Advantage
// =============================================================================

TEST_F(EvalTest, MaterialAdvantage_WhiteUpQueen) {
  // Black queen removed
  Position pos;
  pos.setFromFEN("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  int score = Eval::evaluate(pos);
  // White to move, up a queen (~900 cp material). Should evaluate > +800.
  EXPECT_GT(score, 800) << "White up a queen should be > +800, got " << score;
}

TEST_F(EvalTest, MaterialAdvantage_WhiteUpRook) {
  // Black h8 rook removed
  Position pos;
  pos.setFromFEN("rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1");
  int score = Eval::evaluate(pos);
  // White to move, up a rook (~500 cp). Should evaluate > +400.
  EXPECT_GT(score, 400) << "White up a rook should be > +400, got " << score;
}

TEST_F(EvalTest, MaterialAdvantage_WhiteDownPiece) {
  // White g1 knight removed (black has full pieces)
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
  int score = Eval::evaluate(pos);
  // White to move, down a knight (~320 cp). Should evaluate < -200.
  EXPECT_LT(score, -200) << "White down a piece should be < -200, got " << score;
}

// =============================================================================
// Symmetry
// =============================================================================

TEST_F(EvalTest, Symmetry_StartingPosition) {
  // From white's perspective and from black's perspective, the starting
  // position should evaluate to roughly the same magnitude.
  Position posW;
  posW.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  int scoreW = Eval::evaluate(posW);

  Position posB;
  posB.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
  int scoreB = Eval::evaluate(posB);

  // Both should be near zero; their sum should be near zero (opposite signs
  // from side-to-move perspective, but the position is identical).
  // The eval negates for black, so scoreW and scoreB should have opposite sign
  // but same magnitude. Their sum should be close to zero.
  EXPECT_NEAR(scoreW + scoreB, 0, 10)
      << "Symmetric position should produce symmetric scores: white="
      << scoreW << " black=" << scoreB;
}

// =============================================================================
// Passed Pawn Bonus
// =============================================================================

TEST_F(EvalTest, PassedPawn_BonusOverNoPasser) {
  // KP vs K: white has a passed e-pawn
  Position posPassed;
  posPassed.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  int scorePassed = Eval::evaluate(posPassed);

  // KP vs KP: both sides have a pawn on the same file (neither is passed)
  Position posBlocked;
  posBlocked.setFromFEN("4k3/4p3/8/8/4P3/8/8/4K3 w - - 0 1");
  int scoreBlocked = Eval::evaluate(posBlocked);

  // The passed pawn position should evaluate better for the side to move
  EXPECT_GT(scorePassed, scoreBlocked)
      << "Passed pawn position (" << scorePassed
      << ") should evaluate better than blocked pawn (" << scoreBlocked << ")";
}

// =============================================================================
// King Safety
// =============================================================================

TEST_F(EvalTest, KingSafety_PawnShieldBetterThanOpen) {
  // Simplified position: white king on g1 with intact pawn shield (f2, g2, h2)
  // Black king on g8 with intact pawn shield -- symmetric material
  Position posSafe;
  posSafe.setFromFEN("6k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1");
  int scoreSafe = Eval::evaluate(posSafe);

  // Same material but white's pawn shield is destroyed (no pawns near king)
  // White king on g1, pawns on a2, b2, c2 (far from king)
  // Black king on g8 with intact pawn shield (f7, g7, h7)
  Position posExposed;
  posExposed.setFromFEN("6k1/5ppp/8/8/8/8/PPP5/6K1 w - - 0 1");
  int scoreExposed = Eval::evaluate(posExposed);

  // The position with pawns shielding the king should evaluate better
  EXPECT_GT(scoreSafe, scoreExposed)
      << "King with pawn shield (" << scoreSafe
      << ") should evaluate better than king without shield ("
      << scoreExposed << ")";
}

// =============================================================================
// Mobility
// =============================================================================

TEST_F(EvalTest, Mobility_RestrictedPiecesWorse) {
  // Position where white has good mobility (pieces developed, open center)
  Position posMobile;
  posMobile.setFromFEN(
      "r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
  int scoreMobile = Eval::evaluate(posMobile);

  // Position where white's pieces are cramped behind pawns
  Position posCramped;
  posCramped.setFromFEN(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  int scoreCramped = Eval::evaluate(posCramped);

  // The developed position should be better or equal (white has a knight out
  // and central pawn, black also has a knight out but less central control)
  // We just test that more mobile side does at least as well.
  EXPECT_GE(scoreMobile, scoreCramped)
      << "Developed position (" << scoreMobile
      << ") should evaluate at least as well as starting position ("
      << scoreCramped << ")";
}

// =============================================================================
// Bishop Pair
// =============================================================================

TEST_F(EvalTest, BishopPair_BonusPresent) {
  // White has bishop pair, black has one bishop and one knight
  Position posBishopPair;
  posBishopPair.setFromFEN("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1");
  int scorePair = Eval::evaluate(posBishopPair);

  // White has one bishop and one knight (no bishop pair)
  Position posNoPair;
  posNoPair.setFromFEN("4k3/8/8/8/8/8/8/2B1KN2 w - - 0 1");
  int scoreNoPair = Eval::evaluate(posNoPair);

  // Bishop pair should provide a bonus (approximately 30 cp according to eval)
  // The material is the same (bishop=330, knight=320 so small diff),
  // but bishop pair bonus should make the difference clear.
  EXPECT_GT(scorePair, scoreNoPair)
      << "Bishop pair (" << scorePair
      << ") should evaluate better than bishop+knight (" << scoreNoPair << ")";

  // The bonus should be meaningful (at least 20 cp difference accounting for
  // the small material difference between bishop and knight)
  int diff = scorePair - scoreNoPair;
  EXPECT_GE(diff, 20)
      << "Bishop pair bonus should be at least 20 cp, got " << diff;
}

// =============================================================================
// Development
// =============================================================================

TEST_F(EvalTest, Development_AfterMovesVsStarting) {
  // After 1.e4 e5 2.Nf3 (black to move)
  // White has developed a knight and pushed e4; black has only pushed e5.
  // From black's perspective this should be negative (white is better developed).
  Position posDeveloped;
  posDeveloped.setFromFEN(
      "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
  int scoreDeveloped = Eval::evaluate(posDeveloped);

  // After 1.e4 e5 2.Nf3, the eval from black's perspective should be negative
  // (white has the initiative with one more developed piece).
  EXPECT_LT(scoreDeveloped, 0)
      << "After 1.e4 e5 2.Nf3, eval from black's perspective should be "
         "negative (white better developed), got "
      << scoreDeveloped;
}

TEST_F(EvalTest, Development_AfterE4BetterThanStart) {
  // After 1.e4 (black to move) - white has moved a center pawn
  Position posE4;
  posE4.setFromFEN(
      "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  // Eval from black's side; negate to get white's perspective
  int scoreE4FromBlack = Eval::evaluate(posE4);
  int scoreE4ForWhite = -scoreE4FromBlack;

  // Starting position from white's perspective
  Position posStart;
  posStart.setFromFEN(STARTING_FEN);
  int scoreStartForWhite = Eval::evaluate(posStart);

  // After e4, white should be better than the starting position
  // (center pawn advanced, more space)
  EXPECT_GT(scoreE4ForWhite, scoreStartForWhite)
      << "After 1.e4 white's eval (" << scoreE4ForWhite
      << ") should be better than starting position (" << scoreStartForWhite
      << ")";
}
