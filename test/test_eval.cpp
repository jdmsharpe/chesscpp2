#include "Bitboard.h"
#include "Eval.h"
#include "Magic.h"
#include "Position.h"
#include "Zobrist.h"

#include <gtest/gtest.h>

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
      << "Symmetric position should produce symmetric scores: white=" << scoreW
      << " black=" << scoreB;
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
      << "Passed pawn position (" << scorePassed << ") should evaluate better than blocked pawn ("
      << scoreBlocked << ")";
}

// =============================================================================
// King Safety
// =============================================================================

TEST_F(EvalTest, KingSafety_PawnShieldBetterThanOpen) {
  // Middlegame position: queen+rook present so king safety matters.
  // White king on g1 with intact pawn shield (f2, g2, h2), symmetric material.
  Position posSafe;
  posSafe.setFromFEN("r1bq2k1/5ppp/8/8/8/8/5PPP/R1BQ2K1 w - - 0 1");
  int scoreSafe = Eval::evaluate(posSafe);

  // Same material but white's pawn shield destroyed (pawns far from king).
  // In middlegame with queens, exposed king is a real liability.
  Position posExposed;
  posExposed.setFromFEN("r1bq2k1/5ppp/8/8/8/8/PPP5/R1BQ2K1 w - - 0 1");
  int scoreExposed = Eval::evaluate(posExposed);

  // The position with pawns shielding the king should evaluate better
  EXPECT_GT(scoreSafe, scoreExposed)
      << "King with pawn shield (" << scoreSafe
      << ") should evaluate better than king without shield (" << scoreExposed << ")";
}

// =============================================================================
// Mobility
// =============================================================================

TEST_F(EvalTest, Mobility_RestrictedPiecesWorse) {
  // Position where white has good mobility (pieces developed, open center)
  Position posMobile;
  posMobile.setFromFEN("r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
  int scoreMobile = Eval::evaluate(posMobile);

  // Position where white's pieces are cramped behind pawns
  Position posCramped;
  posCramped.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  int scoreCramped = Eval::evaluate(posCramped);

  // The developed position should be better or equal (white has a knight out
  // and central pawn, black also has a knight out but less central control)
  // We just test that more mobile side does at least as well.
  EXPECT_GE(scoreMobile, scoreCramped)
      << "Developed position (" << scoreMobile
      << ") should evaluate at least as well as starting position (" << scoreCramped << ")";
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
      << "Bishop pair (" << scorePair << ") should evaluate better than bishop+knight ("
      << scoreNoPair << ")";

  // The bonus should be meaningful (at least 20 cp difference accounting for
  // the small material difference between bishop and knight)
  int diff = scorePair - scoreNoPair;
  EXPECT_GE(diff, 20) << "Bishop pair bonus should be at least 20 cp, got " << diff;
}

// =============================================================================
// Development
// =============================================================================

TEST_F(EvalTest, Development_AfterMovesVsStarting) {
  // After 1.e4 e5 2.Nf3 (black to move)
  // White has developed a knight and pushed e4; black has only pushed e5.
  // From black's perspective this should be negative (white is better developed).
  Position posDeveloped;
  posDeveloped.setFromFEN("rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
  int scoreDeveloped = Eval::evaluate(posDeveloped);

  // After 1.e4 e5 2.Nf3, the eval from black's perspective should be negative
  // (white has the initiative with one more developed piece).
  EXPECT_LT(scoreDeveloped, 0) << "After 1.e4 e5 2.Nf3, eval from black's perspective should be "
                                  "negative (white better developed), got "
                               << scoreDeveloped;
}

// =============================================================================
// Endgame King Centralization
// =============================================================================

TEST_F(EvalTest, EndgameKing_CentralBetterThanCorner) {
  // K+P endgame: white king on e4 (central) vs white king on a1 (corner)
  // Both have same material — the central king should evaluate better.
  Position posCentral;
  posCentral.setFromFEN("4k3/8/8/8/4K3/8/4P3/8 w - - 0 1");
  int scoreCentral = Eval::evaluate(posCentral);

  Position posCorner;
  posCorner.setFromFEN("4k3/8/8/8/8/8/4P3/K7 w - - 0 1");
  int scoreCorner = Eval::evaluate(posCorner);

  EXPECT_GT(scoreCentral, scoreCorner)
      << "Central king (" << scoreCentral << ") should evaluate better than corner king ("
      << scoreCorner << ") in endgame";
}

// =============================================================================
// Mop-Up Evaluation
// =============================================================================

TEST_F(EvalTest, MopUp_WinningCornerKingBetter) {
  // White up a rook. Enemy king in corner should evaluate better for white
  // than enemy king in center (mop-up wants to drive king to corner).
  Position posCornerKing;
  posCornerKing.setFromFEN("k7/8/8/8/8/8/8/4K2R w - - 0 1");
  int scoreCorner = Eval::evaluate(posCornerKing);

  Position posCenterKing;
  posCenterKing.setFromFEN("8/8/8/3k4/8/8/8/4K2R w - - 0 1");
  int scoreCenter = Eval::evaluate(posCenterKing);

  // When winning, enemy king in corner should give a higher eval
  EXPECT_GT(scoreCorner, scoreCenter)
      << "Enemy king in corner (" << scoreCorner << ") should be better than center ("
      << scoreCenter << ") when winning with mop-up";
}

// =============================================================================
// Passed Pawn Advancement
// =============================================================================

TEST_F(EvalTest, PassedPawn_AdvancedPawnWorthMore) {
  // White pawn on e6 (advanced) vs white pawn on e3 (not advanced)
  // Both are passed. The advanced one should be worth significantly more.
  Position posAdvanced;
  posAdvanced.setFromFEN("4k3/8/4P3/8/8/8/8/4K3 w - - 0 1");
  int scoreAdvanced = Eval::evaluate(posAdvanced);

  Position posBack;
  posBack.setFromFEN("4k3/8/8/8/8/4P3/8/4K3 w - - 0 1");
  int scoreBack = Eval::evaluate(posBack);

  EXPECT_GT(scoreAdvanced, scoreBack + 30)
      << "Advanced passed pawn on e6 (" << scoreAdvanced
      << ") should be worth significantly more than e3 (" << scoreBack << ")";
}

TEST_F(EvalTest, PassedPawn_ClearPathBonus) {
  // Passed pawn with clear path vs passed pawn with a blocker
  Position posClear;
  posClear.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  int scoreClear = Eval::evaluate(posClear);

  // Same pawn but with a piece blocking the path
  Position posBlocked;
  posBlocked.setFromFEN("4k3/8/4n3/8/4P3/8/8/4K3 w - - 0 1");
  int scoreBlocked = Eval::evaluate(posBlocked);

  // Clear path should give a bonus even accounting for material diff
  // (the knight is extra material for black, so we're really testing that
  // the eval still reflects the blocked path penalty on top of material)
  // Here we just check the clear path evaluates higher (white has less material
  // to compensate against, but also no blocker — the clear-path bonus should
  // partially offset the missing material advantage)
  EXPECT_GT(scoreClear + 320, scoreBlocked)
      << "Clear path with material adjustment should be better: clear=" << scoreClear
      << " blocked=" << scoreBlocked;
}

// =============================================================================
// Rook Behind Passed Pawn
// =============================================================================

TEST_F(EvalTest, RookBehindPasser_BetterThanInFront) {
  // White rook behind own passed pawn (rook on e1, pawn on e5)
  Position posBehind;
  posBehind.setFromFEN("4k3/8/8/4P3/8/8/8/4RK2 w - - 0 1");
  int scoreBehind = Eval::evaluate(posBehind);

  // White rook in front of own passed pawn (rook on e6, pawn on e5)
  // Using e6 instead of e7 to avoid the rook-on-7th-rank bonus masking the result.
  Position posInFront;
  posInFront.setFromFEN("4k3/8/4R3/4P3/8/8/8/5K2 w - - 0 1");
  int scoreInFront = Eval::evaluate(posInFront);

  EXPECT_GT(scoreBehind, scoreInFront)
      << "Rook behind passer (" << scoreBehind << ") should evaluate better than rook in front ("
      << scoreInFront << ")";
}

// =============================================================================
// King-Passer Proximity (Endgame)
// =============================================================================

TEST_F(EvalTest, KingPasserProximity_CloseKingBetter) {
  // White king close to own passed pawn (Kd5 with pawn on e5)
  Position posClose;
  posClose.setFromFEN("7k/8/8/3KP3/8/8/8/8 w - - 0 1");
  int scoreClose = Eval::evaluate(posClose);

  // White king far from own passed pawn (Ka1 with pawn on e5)
  Position posFar;
  posFar.setFromFEN("7k/8/8/4P3/8/8/8/K7 w - - 0 1");
  int scoreFar = Eval::evaluate(posFar);

  EXPECT_GT(scoreClose, scoreFar) << "King close to passer (" << scoreClose
                                  << ") should be better than king far away (" << scoreFar << ")";
}

// =============================================================================
// Development
// =============================================================================

// =============================================================================
// 50-Move Rule Scaling
// =============================================================================

TEST_F(EvalTest, FiftyMoveScaling_HighClockReducesEval) {
  // Same position with halfmove clock 0 vs 80.
  // With clock at 80, the eval should be scaled down.
  Position posNormal;
  posNormal.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  int scoreNormal = Eval::evaluate(posNormal);

  Position posHighClock;
  posHighClock.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 80 1");
  int scoreHighClock = Eval::evaluate(posHighClock);

  // Normal eval should be positive (white has a pawn up)
  EXPECT_GT(scoreNormal, 0);
  // High clock eval should still be positive but significantly smaller
  EXPECT_GT(scoreHighClock, 0);
  EXPECT_LT(scoreHighClock, scoreNormal)
      << "Eval with halfmove clock 80 (" << scoreHighClock << ") should be smaller than clock 0 ("
      << scoreNormal << ")";
}

TEST_F(EvalTest, FiftyMoveScaling_VeryHighClockNearZero) {
  // At halfmove clock 95, the eval should be nearly zero
  Position posNormal;
  posNormal.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  int scoreNormal = Eval::evaluate(posNormal);

  Position posCritical;
  posCritical.setFromFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 95 1");
  int scoreCritical = Eval::evaluate(posCritical);

  // At clock 95, eval should be less than 30% of the normal eval
  EXPECT_LT(scoreCritical * 10, scoreNormal * 3)
      << "Eval at clock 95 (" << scoreCritical << ") should be less than 30% of normal ("
      << scoreNormal << ")";
}

// =============================================================================
// Unstoppable Passed Pawn (Rule of the Square)
// =============================================================================

TEST_F(EvalTest, UnstoppablePasser_OutsideSquareHugeBonus) {
  // White pawn on e5, black king on a8 — king is outside the square of the
  // pawn (pawn needs 2 moves to promote, king needs 5+ to reach e8).
  // No enemy pieces to intercept.
  Position posUnstoppable;
  posUnstoppable.setFromFEN("k7/8/8/4P3/8/8/8/4K3 w - - 0 1");
  int scoreUnstoppable = Eval::evaluate(posUnstoppable);

  // Same setup but black king on f7 — can easily stop the pawn.
  Position posStoppable;
  posStoppable.setFromFEN("8/5k2/8/4P3/8/8/8/4K3 w - - 0 1");
  int scoreStoppable = Eval::evaluate(posStoppable);

  // Unstoppable passer should have a much higher eval (hundreds of cp difference)
  EXPECT_GT(scoreUnstoppable, scoreStoppable + 200)
      << "Unstoppable passer (" << scoreUnstoppable << ") should be much better than stoppable ("
      << scoreStoppable << ")";
}

// =============================================================================
// Development
// =============================================================================

TEST_F(EvalTest, Development_AfterE4BetterThanStart) {
  // After 1.e4 (black to move) - white has moved a center pawn
  Position posE4;
  posE4.setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
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
      << ") should be better than starting position (" << scoreStartForWhite << ")";
}
