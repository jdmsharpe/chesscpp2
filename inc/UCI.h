#pragma once

#include <string>
#include <vector>

#include "Game.h"
#include "Position.h"

// UCI (Universal Chess Interface) protocol handler
class UCI {
 public:
  UCI();
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
