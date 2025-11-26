#pragma once

#include <string>

#include "AI.h"
#include "Position.h"
#include "Types.h"

// Game controller - manages game state and player interactions
class Game {
 public:
  enum GameMode { HUMAN_VS_HUMAN, HUMAN_VS_AI, AI_VS_AI };

  enum GameResult { IN_PROGRESS, WHITE_WINS, BLACK_WINS, DRAW };

  Game(GameMode mode = HUMAN_VS_HUMAN);

  // Game control
  void reset();
  bool makeMove(const std::string& moveStr);
  bool makeMove(Move move);
  Move parseMove(const std::string& moveStr);

  // Get current state
  const Position& getPosition() const { return position; }
  Position& getPosition() { return position; }
  GameMode getMode() const { return mode; }
  GameResult getResult() const { return result; }

  // AI control
  void setAIDepth(int depth) { ai.setDepth(depth); }
  void setAITimeLimit(int ms) { ai.setTimeLimit(ms); }
  void setAIMoveCallback(AI::MoveCallback callback) {
    ai.setMoveCallback(callback);
  }
  void loadOpeningBook(const std::string& filename) {
    ai.loadOpeningBook(filename);
  }
  bool loadPolyglotBook(const std::string& filename) {
    return ai.loadPolyglotBook(filename);
  }
  Move getAIMove();

  // Game status
  bool isGameOver() const { return result != IN_PROGRESS; }
  std::string getResultString() const;

  // Load/save
  bool loadFEN(const std::string& fen);
  std::string saveFEN() const;
  bool loadFromFile(const std::string& filename);
  bool saveToFile(const std::string& filename) const;

 private:
  Position position;
  AI ai;
  GameMode mode;
  GameResult result;

  void updateGameResult();
};
