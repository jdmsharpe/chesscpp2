#include <iostream>
#include <string>

#include "Bitboard.h"
#include "Game.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Position.h"
#include "UCI.h"
#include "Window.h"
#include "Zobrist.h"

void printUsage() {
  std::cout << "Chess++ with Bitboards\n";
  std::cout << "======================\n\n";
  std::cout << "Usage: chesscpp2 [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -h, --help        Show this help message\n";
  std::cout << "  -c, --computer    Play against AI\n";
  std::cout << "  -d, --depth N     Set AI search depth (default: 4)\n";
  std::cout << "  -f, --fen FEN     Load position from FEN string\n";
  std::cout << "  -l, --load FILE   Load position from file\n";
  std::cout << "  --perft N         Run perft test to depth N\n";
  std::cout << "  --nogui           Run in console mode (no GUI)\n";
  std::cout << "  --uci             Run in UCI mode (for GUIs/tournaments)\n";
  std::cout << "\n";
  std::cout << "Controls (GUI mode):\n";
  std::cout << "  Click to select/move pieces\n";
  std::cout << "  Press 'A' for AI to make a move\n";
  std::cout << "  Press 'R' to reset the game\n";
  std::cout << "  Close window or Ctrl+C to quit\n";
}

void runPerft(int depth) {
  std::cout << "Running Perft test to depth " << depth << "...\n\n";

  Position pos;
  pos.setFromFEN(STARTING_FEN);
  pos.print();

  MoveGen::perftDivide(pos, depth);
}

void runConsoleMode(Game& game, int aiDepth) {
  (void)aiDepth;  // Currently unused - for future AI opponent feature
  std::cout << "\nConsole Chess Mode\n";
  std::cout << "==================\n";
  std::cout << "Enter moves in UCI format (e.g., e2e4, e7e8q for promotion)\n";
  std::cout
      << "Type 'quit' to exit, 'fen' to show FEN, 'board' to show board\n\n";

  game.getPosition().print();

  std::string input;
  while (true) {
    // Check for game over
    if (game.isGameOver()) {
      std::cout << "\nGame over: " << game.getResultString() << "\n";
      break;
    }

    // Get input
    Color stm = game.getPosition().sideToMove();
    std::cout << "\n" << (stm == WHITE ? "White" : "Black") << " to move: ";
    std::getline(std::cin, input);

    if (input == "quit" || input == "q") {
      break;
    } else if (input == "fen") {
      std::cout << game.saveFEN() << "\n";
      continue;
    } else if (input == "board") {
      game.getPosition().print();
      continue;
    } else if (input == "ai" || input == "a") {
      // AI makes a move
      std::cout << "AI thinking...\n";
      Move aiMove = game.getAIMove();
      if (aiMove != 0) {
        game.makeMove(aiMove);
        std::cout << "AI played: " << moveToString(aiMove) << "\n";
        game.getPosition().print();
      }
      continue;
    }

    // Try to make the move
    if (game.makeMove(input)) {
      game.getPosition().print();

      // If playing against AI, make AI move
      if (game.getMode() == Game::HUMAN_VS_AI && !game.isGameOver()) {
        std::cout << "\nAI thinking...\n";
        Move aiMove = game.getAIMove();
        if (aiMove != 0) {
          game.makeMove(aiMove);
          std::cout << "AI played: " << moveToString(aiMove) << "\n";
          game.getPosition().print();
        }
      }
    } else {
      std::cout << "Invalid move! Try again.\n";

      // Show legal moves
      std::vector<Move> legalMoves =
          MoveGen::generateLegalMoves(game.getPosition());
      std::cout << "Legal moves: ";
      for (Move m : legalMoves) {
        std::cout << moveToString(m) << " ";
      }
      std::cout << "\n";
    }
  }
}

int main(int argc, char* argv[]) {
  // Parse command line arguments
  bool useAI = false;
  bool useGUI = true;
  bool useUCI = false;
  int aiDepth = 6;
  std::string fenString = "";
  std::string loadFile = "";
  int perftDepth = 0;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage();
      return 0;
    } else if (arg == "-c" || arg == "--computer") {
      useAI = true;
    } else if (arg == "-d" || arg == "--depth") {
      if (i + 1 < argc) {
        aiDepth = std::stoi(argv[++i]);
      }
    } else if (arg == "-f" || arg == "--fen") {
      if (i + 1 < argc) {
        fenString = argv[++i];
      }
    } else if (arg == "-l" || arg == "--load") {
      if (i + 1 < argc) {
        loadFile = argv[++i];
      }
    } else if (arg == "--perft") {
      if (i + 1 < argc) {
        perftDepth = std::stoi(argv[++i]);
      }
    } else if (arg == "--nogui") {
      useGUI = false;
    } else if (arg == "--uci") {
      useUCI = true;
    }
  }

  // Run UCI mode if requested (no initialization output)
  if (useUCI) {
    BB::init();
    Magic::init();
    Zobrist::init();
    UCI uci;
    uci.loop();
    return 0;
  }

  // Initialize bitboard tables
  std::cout << "Initializing bitboard tables...\n";
  BB::init();
  Magic::init();
  Zobrist::init();
  std::cout << "Initialization complete!\n\n";

  // Run perft if requested
  if (perftDepth > 0) {
    runPerft(perftDepth);
    return 0;
  }

  // Create game
  Game::GameMode mode = useAI ? Game::HUMAN_VS_AI : Game::HUMAN_VS_HUMAN;
  Game game(mode);
  game.setAIDepth(aiDepth);

  // Load opening book
  game.loadOpeningBook("../book.txt");

  // Load position if specified
  if (!fenString.empty()) {
    if (!game.loadFEN(fenString)) {
      std::cerr << "Failed to load FEN: " << fenString << "\n";
      return 1;
    }
  } else if (!loadFile.empty()) {
    if (!game.loadFromFile(loadFile)) {
      std::cerr << "Failed to load file: " << loadFile << "\n";
      return 1;
    }
  }

  // Run game
  if (useGUI) {
    // GUI mode
    Window window(800, 800);
    if (!window.init()) {
      std::cerr << "Failed to initialize window!\n";
      std::cerr << "Try running with --nogui for console mode\n";
      return 1;
    }

    std::cout << "\nGUI mode - controls:\n";
    std::cout << "  Click to select/move pieces\n";
    std::cout << "  Press 'A' for AI to make a move\n";
    std::cout << "  Press 'R' to reset the game\n\n";

    window.run(game);
  } else {
    // Console mode
    runConsoleMode(game, aiDepth);
  }

  return 0;
}
