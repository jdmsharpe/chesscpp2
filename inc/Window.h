#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <string>

#include "Game.h"
#include "Types.h"

// SDL2-based GUI window for chess
class Window {
 public:
  Window(int width = 800, int height = 800);
  ~Window();

  // Initialize SDL and create window
  bool init();

  // Main game loop
  void run(Game& game);

  // Clean up
  void cleanup();

 private:
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* piecesTexture;
  int width, height;
  int squareSize;
  int pieceWidth, pieceHeight;  // Size of each sprite in the texture

  // Selected square for move input
  Square selectedSquare;
  bool pieceSelected;

  // AI thinking visualization
  Move currentAIMove;
  int currentAIDepth;
  bool aiThinking;
  Game* currentGame;  // Pointer to current game for AI callback

  // Load piece sprites
  bool loadPieceSprites();

  // Drawing methods
  void draw(const Game& game);
  void drawBoard();
  void drawPieces(const Position& pos);
  void drawSquare(Square sq, SDL_Color color);
  void drawPiece(Piece pc, Square sq);
  void drawHighlights(const Game& game);
  void drawAIThinking();

  // AI callback
  void onAIMoveUpdate(Move move, int depth, const Position& pos);

  // Input handling
  void handleClick(int x, int y, Game& game);
  Square pixelToSquare(int x, int y) const;

  // Get sprite coordinates for a piece
  SDL_Rect getPieceSpriteRect(Piece pc) const;
};
