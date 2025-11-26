#include "UCI.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "AI.h"
#include "Logger.h"
#include "MoveGen.h"
#include "Types.h"

UCI::UCI() : game(Game::HUMAN_VS_AI), searchDepth(6), debug(false) {
  game.setAIDepth(searchDepth);

  // Try multiple paths for opening book (depends on where engine is run from)
  bool bookLoaded = false;
  const char* bookPaths[] = {"book.txt", "../book.txt", "../../book.txt"};
  for (const char* path : bookPaths) {
    std::ifstream test(path);
    if (test.good()) {
      game.loadOpeningBook(path);
      bookLoaded = true;
      Logger::getInstance().info(std::string("UCI: Book loaded from ") + path);
      break;
    }
  }
  if (!bookLoaded) {
    Logger::getInstance().warning("Could not find opening book");
  }
}

UCI::~UCI() {}

void UCI::loop() {
  Logger::getInstance().debug("UCI::loop() starting");

  std::string line;

  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string command;
    iss >> command;

    // Get the rest of the line as arguments
    std::string args;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ') {
      args = args.substr(1);  // Remove leading space
    }

    if (command == "uci") {
      handleUCI();
    } else if (command == "isready") {
      handleIsReady();
    } else if (command == "ucinewgame") {
      handleNewGame();
    } else if (command == "position") {
      handlePosition(args);
    } else if (command == "go") {
      handleGo(args);
    } else if (command == "stop") {
      handleStop();
    } else if (command == "quit") {
      handleQuit();
      break;
    } else if (command == "setoption") {
      handleSetOption(args);
    } else if (command == "d" || command == "display") {
      handleDisplay();
    } else if (debug) {
      std::cout << "info string Unknown command: " << command << std::endl;
    }
  }
}

void UCI::handleUCI() {
  std::cout << "id name Chess++ Bitboards" << std::endl;
  std::cout << "id author Chess++ Team" << std::endl;
  std::cout << "option name Debug type check default false" << std::endl;
  std::cout << "option name Depth type spin default 6 min 1 max 20" << std::endl;
  std::cout << "uciok" << std::endl;
}

void UCI::handleIsReady() { std::cout << "readyok" << std::endl; }

void UCI::handleNewGame() {
  // Don't create new Game object - just reset the position
  // This preserves the opening book that was loaded in the constructor
  game.reset();
  game.setAIDepth(searchDepth);
}

void UCI::handlePosition(const std::string& args) {
  std::istringstream iss(args);
  std::string token;
  iss >> token;

  if (token == "startpos") {
    game.reset();  // Reset position instead of creating new Game
    game.setAIDepth(searchDepth);
    iss >> token;  // Should be "moves" or nothing
  } else if (token == "fen") {
    // Read FEN string (up to "moves" or end of line)
    std::string fen;
    while (iss >> token && token != "moves") {
      if (!fen.empty()) fen += " ";
      fen += token;
    }
    game.reset();  // Reset position instead of creating new Game
    game.setAIDepth(searchDepth);
    if (!game.loadFEN(fen)) {
      if (debug) {
        std::cout << "info string Failed to load FEN: " << fen << std::endl;
      }
      return;
    }
  }

  // Process moves if present
  if (token == "moves") {
    std::string moveStr;
    while (iss >> moveStr) {
      Move move = parseMove(moveStr);
      if (move != 0) {
        if (!game.makeMove(move)) {
          if (debug) {
            std::cout << "info string Illegal move: " << moveStr << std::endl;
          }
          return;
        }
      } else {
        if (debug) {
          std::cout << "info string Failed to parse move: " << moveStr
                    << std::endl;
        }
        return;
      }
    }
  }
}

void UCI::handleGo(const std::string& args) {
  std::istringstream iss(args);
  std::string token;

  int depth = searchDepth;
  int movetime = -1;    // milliseconds (explicit movetime)
  int wtime = -1;       // White's remaining time in ms
  int btime = -1;       // Black's remaining time in ms
  int winc = 0;         // White's increment per move in ms
  int binc = 0;         // Black's increment per move in ms
  int movestogo = -1;   // Moves until next time control
  bool infinite = false;

  // Parse go parameters
  bool depthSpecified = false;
  while (iss >> token) {
    if (token == "depth") {
      iss >> depth;
      depthSpecified = true;
    } else if (token == "movetime") {
      iss >> movetime;
    } else if (token == "wtime") {
      iss >> wtime;
    } else if (token == "btime") {
      iss >> btime;
    } else if (token == "winc") {
      iss >> winc;
    } else if (token == "binc") {
      iss >> binc;
    } else if (token == "movestogo") {
      iss >> movestogo;
    } else if (token == "infinite") {
      infinite = true;
    }
  }

  // Calculate time to use for this move
  int timeForMove = 0;  // 0 means no time limit

  if (!infinite && !depthSpecified) {
    if (movetime > 0) {
      // Explicit movetime - use it directly with small safety margin
      timeForMove = movetime - 50;  // 50ms safety buffer
    } else {
      // Calculate from game time controls
      Color stm = game.getPosition().sideToMove();
      int ourTime = (stm == WHITE) ? wtime : btime;
      int ourInc = (stm == WHITE) ? winc : binc;

      if (ourTime > 0) {
        // Estimate moves remaining if not specified
        int movesLeft = (movestogo > 0) ? movestogo : 30;

        // Basic time allocation: time/moves + increment
        // Use 2.5% of remaining time as base, plus most of the increment
        timeForMove = ourTime / movesLeft + (ourInc * 3 / 4);

        // Don't use more than 10% of remaining time on any single move
        int maxTime = ourTime / 10;
        if (timeForMove > maxTime) {
          timeForMove = maxTime;
        }

        // Minimum time of 100ms, and leave 100ms buffer
        if (timeForMove < 100) {
          timeForMove = 100;
        }
        if (timeForMove > ourTime - 100) {
          timeForMove = ourTime - 100;
        }
        if (timeForMove < 10) {
          timeForMove = 10;  // Absolute minimum
        }

        if (debug) {
          std::cout << "info string Time control: " << ourTime << "ms remaining, "
                    << ourInc << "ms increment, using " << timeForMove << "ms" << std::endl;
        }
      }
    }
  }

  // Set time limit (0 = no limit for infinite or depth-based search)
  game.setAITimeLimit(timeForMove);
  game.setAIDepth(depth);

  if (debug) {
    if (timeForMove > 0) {
      std::cout << "info string Searching with " << timeForMove << "ms time limit" << std::endl;
    } else {
      std::cout << "info string Searching at depth " << depth << std::endl;
    }
  }

  // Get AI move
  Move bestMove = game.getAIMove();

  // Reset time limit after search
  game.setAITimeLimit(0);

  if (bestMove != 0) {
    std::cout << "bestmove " << moveToString(bestMove) << std::endl;
  } else {
    // No legal moves (checkmate or stalemate)
    std::cout << "bestmove 0000" << std::endl;
  }
}

void UCI::handleStop() {
  // Not implemented - would need to interrupt search
  // For now, search completes before responding
}

void UCI::handleQuit() {
  // Clean exit
}

void UCI::handleSetOption(const std::string& args) {
  std::istringstream iss(args);
  std::string token;

  iss >> token;  // "name"
  if (token != "name") return;

  std::string name;
  iss >> name;

  iss >> token;  // "value"
  if (token != "value") return;

  std::string value;
  iss >> value;

  if (name == "Debug") {
    debug = (value == "true");
  } else if (name == "Depth") {
    searchDepth = std::stoi(value);
    game.setAIDepth(searchDepth);
  }
}

void UCI::handleDisplay() {
  game.getPosition().print();
  std::cout << "FEN: " << game.saveFEN() << std::endl;
}

std::vector<std::string> UCI::split(const std::string& str) {
  std::vector<std::string> tokens;
  std::istringstream iss(str);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

Move UCI::parseMove(const std::string& moveStr) {
  // UCI move format: e2e4, e7e8q (promotion)
  if (moveStr.length() < 4) return 0;

  // Parse squares
  int fromFile = moveStr[0] - 'a';
  int fromRank = moveStr[1] - '1';
  int toFile = moveStr[2] - 'a';
  int toRank = moveStr[3] - '1';

  if (fromFile < FILE_A || fromFile > FILE_H || fromRank < RANK_1 ||
      fromRank > RANK_8 || toFile < FILE_A || toFile > FILE_H ||
      toRank < RANK_1 || toRank > RANK_8) {
    return 0;
  }

  Square from = makeSquare(fromFile, fromRank);
  Square to = makeSquare(toFile, toRank);

  // Check for promotion
  PieceType promoType = KNIGHT;
  bool isPromotion = false;

  if (moveStr.length() == 5) {
    isPromotion = true;
    char promoPiece = moveStr[4];
    switch (promoPiece) {
      case 'n':
        promoType = KNIGHT;
        break;
      case 'b':
        promoType = BISHOP;
        break;
      case 'r':
        promoType = ROOK;
        break;
      case 'q':
        promoType = QUEEN;
        break;
      default:
        return 0;
    }
  }

  // Find matching legal move
  std::vector<Move> legalMoves = MoveGen::generateLegalMoves(game.getPosition());

  for (Move move : legalMoves) {
    if (fromSquare(move) == from && toSquare(move) == to) {
      // Check if promotion matches
      if (isPromotion && moveType(move) == PROMOTION) {
        if (promotionType(move) == promoType) {
          return move;
        }
      } else if (!isPromotion && moveType(move) != PROMOTION) {
        return move;
      }
    }
  }

  return 0;
}
