#pragma once

#include "AI.h"
#include "Game.h"
#include "Types.h"

#include <string>
#include <vector>

// UCI (Universal Chess Interface) protocol handler
class UCI {
 public:
  explicit UCI(int initialThreads = AI::DEFAULT_THREADS);
  ~UCI();

  // Main UCI loop - reads commands from stdin and responds
  void loop();

 private:
  Game game;
  int searchDepth;
  bool debug;

  // Command handlers
  void handleUCI();
  void handleIsReady();
  void handleNewGame();
  void handlePosition(const std::string& args);
  void handleGo(const std::string& args);
  void handleStop();
  void handleQuit();
  void handleSetOption(const std::string& args);
  void handleDisplay();  // Debug: display current position

  // Helper functions
  std::vector<std::string> split(const std::string& str);
  Move parseMove(const std::string& moveStr);
};
