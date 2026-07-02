#pragma once

#include "Game.h"
#include "Position.h"
#include "Types.h"

#include <SDL_pixels.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_video.h>
#include <memory>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// RAII deleters for SDL resources
struct SDLWindowDeleter {
  void operator()(SDL_Window* w) const {
    if (w) SDL_DestroyWindow(w);
  }
};
struct SDLRendererDeleter {
  void operator()(SDL_Renderer* r) const {
    if (r) SDL_DestroyRenderer(r);
  }
};
struct SDLTextureDeleter {
  void operator()(SDL_Texture* t) const {
    if (t) SDL_DestroyTexture(t);
  }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using SDLRendererPtr = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;
using SDLTexturePtr = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;

// SDL2-based GUI window for chess
class Window {
 public:
  Window(int width = 800, int height = 800);
  ~Window();

  // Initialize SDL and create window
  bool init();

  // Main game loop
  void run(Game& game);

 private:
  SDLWindowPtr window;
  SDLRendererPtr renderer;
  SDLTexturePtr piecesTexture;
  int width, height;
  int squareSize;
  int pieceWidth, pieceHeight;  // Size of each sprite in the texture

  // Selected square for move input
  Square selectedSquare;
  bool pieceSelected;

  // Cached legal moves (regenerated only when position changes)
  MoveList cachedLegalMoves;
  HashKey cachedPositionKey;

  // AI thinking visualization
  Move currentAIMove;
  int currentAIDepth;
  bool aiThinking;
  Game* currentGame;  // Pointer to current game for AI callback

  // Load piece sprites
  bool loadPieceSprites();

  // Drawing methods
  void draw(Game& game);
  void drawBoard();
  void drawPieces(const Position& pos);
  void drawSquare(Square sq, SDL_Color color);
  void drawPiece(Piece pc, Square sq);
  void drawHighlights(Game& game);
  void drawAIThinking();

  // AI callback
  void onAIMoveUpdate(Move move, int depth, const Position& pos);

  // Input handling
  void handleClick(int x, int y, Game& game);
  [[nodiscard]] Square pixelToSquare(int x, int y) const;

  // Get sprite coordinates for a piece
  [[nodiscard]] SDL_Rect getPieceSpriteRect(Piece pc) const;
};
