#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Bitboard.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Position.h"
#include "Zobrist.h"

// Standard perft test positions with known results
struct PerftTest {
  std::string name;
  std::string fen;
  std::vector<uint64_t> expectedNodes;  // Results for depths 1, 2, 3, 4, 5...
};

// Collection of standard perft test positions
const std::vector<PerftTest> PERFT_TESTS = {
    // Starting position
    {"Starting Position",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {20, 400, 8902, 197281, 4865609, 119060324}},

    // Kiwipete position (complex midgame)
    {"Kiwipete",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {48, 2039, 97862, 4085603, 193690690}},

    // Position 3 (difficult position with many checks)
    {"Position 3",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {14, 191, 2812, 43238, 674624, 11030083, 178633661}},

    // Position 4 (complex endgame)
    {"Position 4",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {6, 264, 9467, 422333, 15833292, 706045033}},

    // Position 5 (asymmetric position)
    {"Position 5",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {44, 1486, 62379, 2103487, 89941194}},

    // Position 6 (Edwards2)
    {"Position 6",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {46, 2079, 89890, 3894594, 164075551}},
};

// Format large numbers with commas
std::string formatNumber(uint64_t n) {
  std::string s = std::to_string(n);
  int insertPosition = s.length() - 3;
  while (insertPosition > 0) {
    s.insert(insertPosition, ",");
    insertPosition -= 3;
  }
  return s;
}

// Format time in appropriate units
std::string formatTime(double ms) {
  if (ms < 1.0) {
    return std::to_string(static_cast<int>(ms * 1000)) + " μs";
  } else if (ms < 1000.0) {
    return std::to_string(static_cast<int>(ms)) + " ms";
  } else {
    return std::to_string(ms / 1000.0).substr(0, 5) + " s";
  }
}

// Run perft test with timing
struct PerftResult {
  uint64_t nodes;
  double timeMs;
  bool correct;
};

PerftResult runPerftTest(const std::string& fen, int depth,
                         uint64_t expectedNodes) {
  Position pos;
  pos.setFromFEN(fen);

  auto start = std::chrono::high_resolution_clock::now();
  uint64_t nodes = MoveGen::perft(pos, depth);
  auto end = std::chrono::high_resolution_clock::now();

  double timeMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  return {nodes, timeMs, nodes == expectedNodes};
}

// Run a single perft test suite
void runTestSuite(const PerftTest& test, int maxDepth) {
  std::cout << "\n┌───────────────────────────────────────────────────────────────"
               "─────────────┐\n";
  std::cout << "│ " << std::left << std::setw(70) << test.name << " │\n";
  std::cout << "└───────────────────────────────────────────────────────────────"
               "─────────────┘\n";
  std::cout << "FEN: " << test.fen << "\n\n";

  std::cout << "Depth │    Nodes    │ Expected   │  Time    │   Nodes/s   │ "
               "Result\n";
  std::cout << "──────┼─────────────┼────────────┼──────────┼─────────────┼──"
               "──────\n";

  int testCount = 0;
  int passCount = 0;

  for (int depth = 1; depth <= maxDepth; depth++) {
    if (depth - 1 >= static_cast<int>(test.expectedNodes.size())) {
      break;
    }

    uint64_t expected = test.expectedNodes[depth - 1];
    PerftResult result = runPerftTest(test.fen, depth, expected);

    uint64_t nodesPerSecond =
        (result.timeMs > 0) ? (result.nodes * 1000.0 / result.timeMs) : 0;

    std::cout << std::right << std::setw(5) << depth << " │ ";
    std::cout << std::right << std::setw(11) << formatNumber(result.nodes)
              << " │ ";
    std::cout << std::right << std::setw(10) << formatNumber(expected) << " │ ";
    std::cout << std::right << std::setw(8) << formatTime(result.timeMs)
              << " │ ";
    std::cout << std::right << std::setw(11) << formatNumber(nodesPerSecond)
              << " │ ";
    std::cout << (result.correct ? "✓ PASS" : "✗ FAIL") << "\n";

    testCount++;
    if (result.correct) passCount++;
  }

  std::cout << "\nSummary: " << passCount << "/" << testCount
            << " tests passed\n";
}

// Run all tests
void runAllTests(int maxDepth) {
  std::cout << "\n╔═══════════════════════════════════════════════════════════════"
               "═════════════╗\n";
  std::cout << "║                     CHESS++ PERFT BENCHMARK SUITE              "
               "            ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════"
               "═════════════╝\n";

  int totalTests = 0;
  int totalPassed = 0;

  for (const auto& test : PERFT_TESTS) {
    runTestSuite(test, maxDepth);

    // Count how many tests passed
    for (int depth = 1;
         depth <= maxDepth && depth <= static_cast<int>(test.expectedNodes.size());
         depth++) {
      totalTests++;
      uint64_t expected = test.expectedNodes[depth - 1];
      Position pos;
      pos.setFromFEN(test.fen);
      uint64_t nodes = MoveGen::perft(pos, depth);
      if (nodes == expected) totalPassed++;
    }
  }

  std::cout << "\n╔═══════════════════════════════════════════════════════════════"
               "═════════════╗\n";
  std::cout << "║ FINAL RESULTS: " << std::setw(3) << totalPassed << "/"
            << std::setw(3) << totalTests
            << " tests passed                                        ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════"
               "═════════════╝\n\n";
}

// Run quick benchmark on starting position
void runQuickBenchmark() {
  std::cout << "\n╔═══════════════════════════════════════════════════════════════"
               "═════════════╗\n";
  std::cout << "║                     QUICK PERFORMANCE BENCHMARK                "
               "            ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════"
               "═════════════╝\n\n";

  Position pos;
  pos.setFromFEN(STARTING_FEN);

  std::cout << "Testing starting position at increasing depths...\n\n";
  std::cout << "Depth │     Nodes      │   Time    │    Nodes/second\n";
  std::cout << "──────┼────────────────┼───────────┼─────────────────\n";

  for (int depth = 1; depth <= 6; depth++) {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t nodes = MoveGen::perft(pos, depth);
    auto end = std::chrono::high_resolution_clock::now();

    double timeMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    uint64_t nps = (timeMs > 0) ? (nodes * 1000.0 / timeMs) : 0;

    std::cout << std::right << std::setw(5) << depth << " │ ";
    std::cout << std::right << std::setw(14) << formatNumber(nodes) << " │ ";
    std::cout << std::right << std::setw(9) << formatTime(timeMs) << " │ ";
    std::cout << std::right << std::setw(14) << formatNumber(nps) << "\n";

    // Stop if it's taking too long
    if (timeMs > 30000) {
      std::cout << "\nStopping benchmark (depth " << depth
                << " took > 30 seconds)\n";
      break;
    }
  }

  std::cout << "\n";
}

int main(int argc, char* argv[]) {
  // Initialize
  BB::init();
  Magic::init();
  Zobrist::init();

  // Parse arguments
  bool quickMode = false;
  int maxDepth = 5;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--quick" || arg == "-q") {
      quickMode = true;
    } else if (arg == "--depth" || arg == "-d") {
      if (i + 1 < argc) {
        maxDepth = std::stoi(argv[++i]);
      }
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Chess++ Perft Test Suite\n\n";
      std::cout << "Usage: perft_suite [options]\n\n";
      std::cout << "Options:\n";
      std::cout << "  -q, --quick       Run quick benchmark only (starting "
                   "position)\n";
      std::cout
          << "  -d, --depth N     Maximum depth to test (default: 5)\n";
      std::cout << "  -h, --help        Show this help message\n";
      return 0;
    }
  }

  if (quickMode) {
    runQuickBenchmark();
  } else {
    runAllTests(maxDepth);
  }

  return 0;
}
