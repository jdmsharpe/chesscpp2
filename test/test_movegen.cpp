#include <gtest/gtest.h>

#include <algorithm>

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

// ============================================================
// generateCheckingMoves() tests
// ============================================================

TEST_F(MoveGenTest, CheckingMoves_NoneFromStartingPosition) {
  // From the starting position no non-capture move gives check.
  Position pos;
  pos.setFromFEN(STARTING_FEN);

  std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
  EXPECT_TRUE(checks.empty());
}

TEST_F(MoveGenTest, CheckingMoves_KnightGivesCheck) {
  // Nf3 can move to squares that give check to the black king on e8.
  // Nd4 gives check (d4 attacks e6? no). Let's reason:
  //   Knight on f3 can go to: e5, g5, d4, h4, d2, h2, e1, g1
  //   King on e8 — attacked by knight from d6, f6, c7, g7
  //   So Nf3-d4 does NOT check. We need Nf3-e5? No. Nf3-g5? No.
  //   Better FEN: knight on d5, king on e8 => Nc7+ or Nf6+ give check.
  Position pos;
  pos.setFromFEN("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1");

  std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
  EXPECT_FALSE(checks.empty());

  // Nc7+ and Nf6+ should both give check to ke8
  bool foundNc7 = false;
  bool foundNf6 = false;
  for (Move m : checks) {
    Square from = fromSquare(m);
    Square to = toSquare(m);
    if (from == D5 && to == C7) foundNc7 = true;
    if (from == D5 && to == F6) foundNf6 = true;
  }
  EXPECT_TRUE(foundNc7) << "Nd5-c7+ should be a checking move";
  EXPECT_TRUE(foundNf6) << "Nd5-f6+ should be a checking move";
}

TEST_F(MoveGenTest, CheckingMoves_DiscoveredCheck) {
  // Rook on e1, knight on e4, black king on e8.
  // Moving the knight off the e-file discovers check from the rook.
  Position pos;
  pos.setFromFEN("4k3/8/8/8/4N3/8/8/4K1R1 w - - 0 1");
  // Rook on g1, knight on e4, kings on e1/e8 — no discovered check there.
  // Let's use: rook on e1-file behind the knight.
  // FEN: "4k3/8/8/8/4N3/8/8/R3K3 w Q - 0 1"
  // Rook on a1, no. We need rook on e-file.
  // "4k3/8/8/8/4N3/8/8/4K3 w - - 0 1" — only king and knight, no rook.
  // Correct FEN: Rook on e1, King on d1, Knight on e4, Black King on e8.
  pos.setFromFEN("4k3/8/8/8/4N3/8/8/3RK3 w - - 0 1");
  // Rook on d1 — that's d-file, not e-file. Let me be precise:
  // King on a1, Rook on e1, Knight on e4, Black king on e8.
  pos.setFromFEN("4k3/8/8/8/4N3/8/8/K3R3 w - - 0 1");

  std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);

  // Any knight move off the e-file discovers check from Re1 to ke8.
  // Knight on e4 can move to: d6, f6, c5, g5, c3, g3, d2, f2
  // All of these leave the e-file open for the rook.
  // Some of these may also be direct knight checks (d6, f6 check the king).
  // All should appear as checking moves.
  EXPECT_GE(checks.size(), 6u)
      << "Multiple discovered checks should be found";

  // Verify at least one pure discovered check (not a direct knight check)
  // c5, g5, c3, g3, d2, f2 are discovered-only checks
  bool foundDiscovered = false;
  for (Move m : checks) {
    Square to = toSquare(m);
    if (to == C5 || to == G5 || to == C3 || to == G3 || to == D2 || to == F2) {
      foundDiscovered = true;
      break;
    }
  }
  EXPECT_TRUE(foundDiscovered)
      << "Should find at least one pure discovered check";
}

TEST_F(MoveGenTest, CheckingMoves_NoCapturesInResults) {
  // Position where a piece can capture or give check.
  // White Nd5 with black pawn on c7 and black king on e8.
  // Nd5xc7+ is a capture-check; Nd5-f6+ is a quiet check.
  // generateCheckingMoves should NOT include the capture.
  Position pos;
  pos.setFromFEN("4k3/2p5/8/3N4/8/8/8/4K3 w - - 0 1");

  std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);

  for (Move m : checks) {
    Square to = toSquare(m);
    // The move should not be a capture (c7 has a black pawn)
    EXPECT_NE(to, C7) << "Capture Nd5xc7 should not appear in checking moves";
    // Also verify via moveType that no en passant sneaks in
    EXPECT_NE(moveType(m), EN_PASSANT)
        << "En passant captures should not appear in checking moves";
  }

  // But Nf6+ should still be present as a quiet check
  bool foundNf6 = false;
  for (Move m : checks) {
    if (fromSquare(m) == D5 && toSquare(m) == F6) {
      foundNf6 = true;
    }
  }
  EXPECT_TRUE(foundNf6) << "Nd5-f6+ quiet check should be present";
}

TEST_F(MoveGenTest, CheckingMoves_AllReturnedMovesAreLegal) {
  // Test across several positions that every returned move is legal
  // (i.e., our own king is not left in check after making the move).
  std::vector<std::string> fens = {
      STARTING_FEN,
      "4k3/8/8/3N4/8/8/8/4K3 w - - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "4k3/8/8/8/4N3/8/8/K3R3 w - - 0 1",
  };

  for (const auto& fen : fens) {
    Position pos;
    pos.setFromFEN(fen);

    std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
    Color us = pos.sideToMove();

    for (Move m : checks) {
      pos.makeMove(m);
      // After making the move, our king should NOT be in check (legal move)
      bool ourKingInCheck =
          pos.isAttacked(BB::lsb(pos.pieces(us, KING)), pos.sideToMove());
      EXPECT_FALSE(ourKingInCheck)
          << "Move " << moveToString(m) << " in position " << fen
          << " leaves our king in check (illegal)";
      pos.unmakeMove();
    }
  }
}

TEST_F(MoveGenTest, CheckingMoves_AllReturnedMovesGiveCheck) {
  // Every move returned by generateCheckingMoves should give check.
  std::vector<std::string> fens = {
      "4k3/8/8/3N4/8/8/8/4K3 w - - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "4k3/8/8/8/4N3/8/8/K3R3 w - - 0 1",
      "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
  };

  for (const auto& fen : fens) {
    Position pos;
    pos.setFromFEN(fen);

    std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);

    for (Move m : checks) {
      pos.makeMove(m);
      EXPECT_TRUE(pos.inCheck())
          << "Move " << moveToString(m) << " in position " << fen
          << " should give check but does not";
      pos.unmakeMove();
    }
  }
}

TEST_F(MoveGenTest, CheckingMoves_BruteForceComparison) {
  // Compare generateCheckingMoves against brute-force approach:
  // generate all legal moves, filter for non-captures that give check.
  std::vector<std::string> fens = {
      STARTING_FEN,
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
      "4k3/8/8/3N4/8/8/8/4K3 w - - 0 1",
      "4k3/8/8/8/4N3/8/8/K3R3 w - - 0 1",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
  };

  for (const auto& fen : fens) {
    Position pos;
    pos.setFromFEN(fen);

    // Method 1: the function under test
    std::vector<Move> checkingMoves = MoveGen::generateCheckingMoves(pos);

    // Method 2: brute force — all legal non-captures that give check
    std::vector<Move> legalMoves = MoveGen::generateLegalMoves(pos);
    std::vector<Move> bruteForceChecks;
    for (Move m : legalMoves) {
      Square to = toSquare(m);
      // Skip captures and en passant (same filter as generateCheckingMoves)
      if (pos.pieceAt(to) != NO_PIECE || moveType(m) == EN_PASSANT) {
        continue;
      }
      pos.makeMove(m);
      if (pos.inCheck()) {
        bruteForceChecks.push_back(m);
      }
      pos.unmakeMove();
    }

    // Sort both vectors for comparison
    std::vector<Move> sortedChecking(checkingMoves.begin(),
                                     checkingMoves.end());
    std::vector<Move> sortedBrute(bruteForceChecks.begin(),
                                  bruteForceChecks.end());
    std::sort(sortedChecking.begin(), sortedChecking.end());
    std::sort(sortedBrute.begin(), sortedBrute.end());

    EXPECT_EQ(sortedChecking.size(), sortedBrute.size())
        << "Mismatch in count for position: " << fen;
    EXPECT_EQ(sortedChecking, sortedBrute)
        << "Mismatch in moves for position: " << fen;
  }
}
