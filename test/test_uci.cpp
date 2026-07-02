#include "Bitboard.h"
#include "Magic.h"
#include "Polyglot.h"
#include "PolyglotTestUtils.h"
#include "Position.h"
#include "UCI.h"
#include "Zobrist.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// Helper: Feed commands to UCI::loop() via stdin redirection,
// capture everything written to stdout, return it as a string.
static std::string runUCI(const std::string& commands) {
  // Save original buffers
  std::streambuf* origIn = std::cin.rdbuf();
  std::streambuf* origOut = std::cout.rdbuf();

  // Redirect cin from our command string (always end with "quit")
  std::istringstream input(commands + "\nquit\n");
  std::cin.rdbuf(input.rdbuf());

  // Redirect cout to capture output
  std::ostringstream output;
  std::cout.rdbuf(output.rdbuf());

  {
    UCI uci;
    uci.loop();
  }

  // Restore original buffers
  std::cin.rdbuf(origIn);
  std::cout.rdbuf(origOut);

  return output.str();
}

class UCITest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

namespace {

std::vector<std::string> collectBestMoves(const std::string& output) {
  std::vector<std::string> moves;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.rfind("bestmove ", 0) == 0 && line.size() >= 13) {
      moves.push_back(line.substr(9, 4));
    }
  }
  return moves;
}

}  // namespace

// ---------- Test 1: UCI identification ----------
TEST_F(UCITest, UCIIdentification) {
  std::string output = runUCI("uci");

  // Must contain engine name
  EXPECT_NE(output.find("id name"), std::string::npos) << "Missing 'id name' in UCI output";

  // Must contain author
  EXPECT_NE(output.find("id author"), std::string::npos) << "Missing 'id author' in UCI output";

  // Must end the handshake with uciok
  EXPECT_NE(output.find("uciok"), std::string::npos) << "Missing 'uciok' in UCI output";
}

// ---------- Test 2: isready responds readyok ----------
TEST_F(UCITest, IsReadyRespondsReadyOK) {
  std::string output = runUCI("isready");

  EXPECT_NE(output.find("readyok"), std::string::npos)
      << "Missing 'readyok' in response to 'isready'";
}

// ---------- Test 3: Position startpos parsing ----------
TEST_F(UCITest, PositionStartpos) {
  // After "position startpos", ask the engine to display the position.
  // The 'd' command outputs the FEN, which should be the starting FEN.
  std::string output = runUCI("position startpos\nd");

  EXPECT_NE(output.find("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq"), std::string::npos)
      << "Position after 'position startpos' does not match starting FEN.\n"
      << "Output was: " << output;
}

// ---------- Test 4: Position startpos with moves ----------
TEST_F(UCITest, PositionStartposWithMoves) {
  // After "position startpos moves e2e4 e7e5", verify the position
  // reflects those two moves played.
  std::string output = runUCI("position startpos moves e2e4 e7e5\nd");

  // After 1.e4 e5, the FEN should show pawns on e4 and e5, black to move
  // is wrong -- it should be white to move after two moves.
  // Expected FEN: rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq
  EXPECT_NE(output.find("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq"),
            std::string::npos)
      << "Position after 'position startpos moves e2e4 e7e5' is incorrect.\n"
      << "Output was: " << output;
}

// ---------- Test 5: Position FEN parsing ----------
TEST_F(UCITest, PositionFEN) {
  // Set a specific FEN and verify it was loaded correctly.
  const std::string fen = "r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2";
  std::string output = runUCI("position fen " + fen + "\nd");

  // The display output should contain the piece placement from our FEN
  EXPECT_NE(output.find("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR"), std::string::npos)
      << "Position FEN not correctly loaded.\nOutput was: " << output;
}

// ---------- Test 6: Go depth produces bestmove ----------
TEST_F(UCITest, GoDepthProducesBestmove) {
  // From the starting position, "go depth 1" should produce a bestmove.
  std::string output = runUCI("position startpos\ngo depth 1");

  EXPECT_NE(output.find("bestmove"), std::string::npos)
      << "Missing 'bestmove' in response to 'go depth 1'.\n"
      << "Output was: " << output;

  // The bestmove should be a valid 4-character move (e.g., "e2e4")
  auto pos = output.find("bestmove ");
  ASSERT_NE(pos, std::string::npos);
  std::string moveStr = output.substr(pos + 9, 4);
  // Basic format check: two file-rank pairs
  EXPECT_GE(moveStr[0], 'a');
  EXPECT_LE(moveStr[0], 'h');
  EXPECT_GE(moveStr[1], '1');
  EXPECT_LE(moveStr[1], '8');
  EXPECT_GE(moveStr[2], 'a');
  EXPECT_LE(moveStr[2], 'h');
  EXPECT_GE(moveStr[3], '1');
  EXPECT_LE(moveStr[3], '8');
}

// ---------- Test 7: Setoption Depth ----------
TEST_F(UCITest, SetOptionDepth) {
  // Set depth to 1 via setoption, then do a search.
  // If the depth option works, we should get a bestmove back quickly.
  std::string output = runUCI(
      "setoption name Depth value 1\n"
      "position startpos\n"
      "go depth 1");

  EXPECT_NE(output.find("bestmove"), std::string::npos)
      << "Missing 'bestmove' after setting depth option.\n"
      << "Output was: " << output;
}

// ---------- Test 8: UCI full handshake sequence ----------
TEST_F(UCITest, FullHandshakeSequence) {
  // Typical GUI handshake: uci -> (wait for uciok) -> isready -> readyok
  std::string output = runUCI("uci\nisready");

  // uciok should appear before readyok
  auto uciokPos = output.find("uciok");
  auto readyokPos = output.find("readyok");

  ASSERT_NE(uciokPos, std::string::npos) << "Missing 'uciok'";
  ASSERT_NE(readyokPos, std::string::npos) << "Missing 'readyok'";
  EXPECT_LT(uciokPos, readyokPos) << "'uciok' should appear before 'readyok' in the output";
}

// ---------- Test 9: UCI options advertised ----------
TEST_F(UCITest, UCIOptionsAdvertised) {
  std::string output = runUCI("uci");

  // The engine should advertise its configurable options
  EXPECT_NE(output.find("option name Threads type spin default 4"), std::string::npos)
      << "Missing Threads option with the expected default in UCI output";
  EXPECT_NE(output.find("option name Depth type spin default 4"), std::string::npos)
      << "Missing Depth option with the expected default in UCI output";
  EXPECT_NE(output.find("option name Depth"), std::string::npos)
      << "Missing Depth option in UCI output";
  EXPECT_NE(output.find("option name SyzygyPath"), std::string::npos)
      << "Missing SyzygyPath option in UCI output";
  EXPECT_NE(output.find("option name BookPath"), std::string::npos)
      << "Missing BookPath option in UCI output";
}

// ---------- Test 10: Position FEN with moves ----------
TEST_F(UCITest, PositionFENWithMoves) {
  // Load a FEN, then apply a move from that position.
  // Start from a position where it's white's turn with a pawn on e2.
  std::string output = runUCI(
      "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
      "moves e2e4\n"
      "d");

  // After 1.e4 from starting position, black to move
  EXPECT_NE(output.find("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq"), std::string::npos)
      << "Position FEN with moves not correctly applied.\n"
      << "Output was: " << output;
}

// ---------- Test 11: ucinewgame resets position ----------
TEST_F(UCITest, UCINewGameResetsPosition) {
  // Make some moves, then ucinewgame should reset back to start
  std::string output = runUCI(
      "position startpos moves e2e4 e7e5 g1f3\n"
      "ucinewgame\n"
      "d");

  EXPECT_NE(output.find("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq"), std::string::npos)
      << "Position not reset after ucinewgame.\nOutput was: " << output;
}

// ---------- Test 12: Go depth from non-starting position ----------
TEST_F(UCITest, GoDepthFromCustomPosition) {
  // From a position with limited material, go depth 1 should still work
  std::string output = runUCI(
      "position fen k7/8/1K6/8/8/8/8/7R w - - 0 1\n"
      "go depth 1");

  EXPECT_NE(output.find("bestmove"), std::string::npos)
      << "Missing 'bestmove' from custom position.\n"
      << "Output was: " << output;
}

TEST_F(UCITest, RepeatedPolyglotQueriesStayStable) {
  Position pos;
  pos.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  testutil::ScopedTempBookFile bookFile("uci_polyglot_repro");
  const uint64_t key = PolyglotBook::computeHash(pos);
  ASSERT_TRUE(testutil::writePolyglotBook(bookFile.path(),
                                          {
                                              {key, testutil::encodePolyglotMove("e2e4"), 30, 0},
                                              {key, testutil::encodePolyglotMove("d2d4"), 20, 0},
                                          }));

  std::ostringstream commands;
  commands << "setoption name Debug value true\n";
  commands << "setoption name BookPath value " << bookFile.path().string() << "\n";
  for (int i = 0; i < 25; ++i) {
    commands << "position startpos\n";
    commands << "go depth 1\n";
  }

  const std::string output = runUCI(commands.str());
  const auto bestMoves = collectBestMoves(output);

  ASSERT_EQ(bestMoves.size(), 25u) << output;
  for (const auto& move : bestMoves) {
    EXPECT_TRUE(move == "e2e4" || move == "d2d4") << "Unexpected bestmove: " << move;
  }
}
