#include "Window.h"

#include <iostream>

#include "MoveGen.h"

Window::Window(int width, int height)
    : window(nullptr),
      renderer(nullptr),
      piecesTexture(nullptr),
      width(width),
      height(height),
      selectedSquare(NO_SQUARE),
      pieceSelected(false),
      currentAIMove(0),
      currentAIDepth(0),
      aiThinking(false),
      currentGame(nullptr) {
  squareSize = width / 8;
}

Window::~Window() { cleanup(); }

bool Window::init() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
    return false;
  }

  // Initialize SDL_image for PNG loading
  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    std::cerr << "SDL_image initialization failed: " << IMG_GetError() << "\n";
    return false;
  }

  window =
      SDL_CreateWindow("Chess++ with Bitboards", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);

  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
    return false;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
    return false;
  }

  // Load piece sprites
  if (!loadPieceSprites()) {
    std::cerr << "Failed to load piece sprites\n";
    return false;
  }

  return true;
}

bool Window::loadPieceSprites() {
  // Try multiple possible paths for the pieces.png file
  const char* paths[] = {"inc/pieces.png", "../inc/pieces.png",
                         "../../inc/pieces.png", "pieces.png"};

  SDL_Surface* surface = nullptr;
  for (const char* path : paths) {
    surface = IMG_Load(path);
    if (surface) {
      std::cout << "Loaded pieces from: " << path << "\n";
      break;
    }
  }

  if (!surface) {
    std::cerr << "Failed to load pieces.png: " << IMG_GetError() << "\n";
    return false;
  }

  piecesTexture = SDL_CreateTextureFromSurface(renderer, surface);

  // Get texture dimensions
  pieceWidth = surface->w / 6;   // 6 pieces per row
  pieceHeight = surface->h / 2;  // 2 rows (white and black)

  SDL_FreeSurface(surface);

  if (!piecesTexture) {
    std::cerr << "Failed to create texture from pieces.png: " << SDL_GetError()
              << "\n";
    return false;
  }

  return true;
}

void Window::cleanup() {
  if (piecesTexture) {
    SDL_DestroyTexture(piecesTexture);
    piecesTexture = nullptr;
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  IMG_Quit();
  SDL_Quit();
}

void Window::run(Game& game) {
  bool running = true;
  SDL_Event event;

  // Store game pointer for AI callback
  currentGame = &game;

  // Set up AI callback for live visualization
  game.setAIMoveCallback([this](Move move, int depth, const Position& pos) {
    this->onAIMoveUpdate(move, depth, pos);
  });

  while (running) {
    // Handle events
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 event.button.button == SDL_BUTTON_LEFT) {
        handleClick(event.button.x, event.button.y, game);
      } else if (event.type == SDL_KEYDOWN) {
        // 'R' to reset
        if (event.key.keysym.sym == SDLK_r) {
          game.reset();
          selectedSquare = NO_SQUARE;
          pieceSelected = false;
        }
        // 'A' for AI move
        else if (event.key.keysym.sym == SDLK_a) {
          if (!game.isGameOver()) {
            std::cout << "AI thinking...\n";
            aiThinking = true;
            currentAIMove = 0;
            currentAIDepth = 0;
            Move aiMove = game.getAIMove();
            aiThinking = false;
            if (aiMove != 0) {
              game.makeMove(aiMove);
              std::cout << "AI played: " << moveToString(aiMove) << "\n";
            }
          }
        }
      }
    }

    // Draw
    draw(game);

    // Cap framerate
    SDL_Delay(16);  // ~60 FPS
  }
}

void Window::onAIMoveUpdate(Move move, int depth, const Position& pos) {
  (void)depth;  // Unused for now
  currentAIMove = move;
  currentAIDepth = depth;

  if (!currentGame) return;

  // Process pending SDL events to keep window responsive
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      // Re-push quit event so main loop can handle it
      SDL_PushEvent(&event);
      return;
    }
  }

  // Immediately redraw with current move highlighted
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderClear(renderer);

  // Draw board
  drawBoard();

  // Draw pieces from the position
  drawPieces(pos);

  // Draw AI thinking overlay
  if (currentAIMove != 0) {
    Square from = fromSquare(currentAIMove);
    Square to = toSquare(currentAIMove);

    // Highlight with bright purple/magenta
    SDL_Color aiMoveColor = {255, 100, 255, 150};

    // Draw "from" square highlight
    int fromFile = fileOf(from);
    int fromRank = 7 - rankOf(from);
    SDL_Rect fromRect;
    fromRect.x = fromFile * squareSize;
    fromRect.y = fromRank * squareSize;
    fromRect.w = squareSize;
    fromRect.h = squareSize;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, aiMoveColor.r, aiMoveColor.g,
                           aiMoveColor.b, 100);
    SDL_RenderFillRect(renderer, &fromRect);

    // Draw "to" square highlight
    int toFile = fileOf(to);
    int toRank = 7 - rankOf(to);
    SDL_Rect toRect;
    toRect.x = toFile * squareSize;
    toRect.y = toRank * squareSize;
    toRect.w = squareSize;
    toRect.h = squareSize;
    SDL_SetRenderDrawColor(renderer, aiMoveColor.r, aiMoveColor.g,
                           aiMoveColor.b, 150);
    SDL_RenderFillRect(renderer, &toRect);

    // Draw thick arrow from -> to
    SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255);
    int fromX = fromFile * squareSize + squareSize / 2;
    int fromY = fromRank * squareSize + squareSize / 2;
    int toX = toFile * squareSize + squareSize / 2;
    int toY = toRank * squareSize + squareSize / 2;

    // Draw multiple lines for thickness
    for (int offset = -3; offset <= 3; ++offset) {
      SDL_RenderDrawLine(renderer, fromX + offset, fromY, toX + offset, toY);
      SDL_RenderDrawLine(renderer, fromX, fromY + offset, toX, toY + offset);
    }
  }

  // Present immediately (don't wait for main loop)
  SDL_RenderPresent(renderer);
}

void Window::draw(const Game& game) {
  // Clear screen
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderClear(renderer);

  // Draw board and pieces
  drawBoard();
  drawHighlights(game);
  drawPieces(game.getPosition());

  // Draw AI thinking visualization
  if (aiThinking) {
    drawAIThinking();
  }

  // Present
  SDL_RenderPresent(renderer);
}

void Window::drawBoard() {
  SDL_Color lightSquare = {240, 217, 181, 255};
  SDL_Color darkSquare = {181, 136, 99, 255};

  for (int rank = 0; rank < 8; ++rank) {
    for (int file = 0; file < 8; ++file) {
      SDL_Color color = ((rank + file) % 2 == 0) ? lightSquare : darkSquare;
      Square sq = makeSquare(file, 7 - rank);  // Flip for display
      drawSquare(sq, color);
    }
  }
}

void Window::drawSquare(Square sq, SDL_Color color) {
  int file = fileOf(sq);
  int rank = 7 - rankOf(sq);  // Flip for display

  SDL_Rect rect;
  rect.x = file * squareSize;
  rect.y = rank * squareSize;
  rect.w = squareSize;
  rect.h = squareSize;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void Window::drawHighlights(const Game& game) {
  if (pieceSelected && selectedSquare != NO_SQUARE) {
    // Highlight selected square
    SDL_Color highlight = {100, 200, 100, 100};

    int file = fileOf(selectedSquare);
    int rank = 7 - rankOf(selectedSquare);

    SDL_Rect rect;
    rect.x = file * squareSize;
    rect.y = rank * squareSize;
    rect.w = squareSize;
    rect.h = squareSize;

    SDL_SetRenderDrawColor(renderer, highlight.r, highlight.g, highlight.b,
                           128);
    SDL_RenderFillRect(renderer, &rect);

    // Highlight legal move squares
    // Need mutable copy to generate legal moves
    Game& mutableGame = const_cast<Game&>(game);
    std::vector<Move> legalMoves =
        MoveGen::generateLegalMoves(mutableGame.getPosition());
    SDL_Color legalMoveColor = {200, 200, 100, 100};

    for (Move move : legalMoves) {
      if (fromSquare(move) == selectedSquare) {
        Square to = toSquare(move);
        int toFile = fileOf(to);
        int toRank = 7 - rankOf(to);

        SDL_Rect toRect;
        toRect.x = toFile * squareSize + squareSize / 3;
        toRect.y = toRank * squareSize + squareSize / 3;
        toRect.w = squareSize / 3;
        toRect.h = squareSize / 3;

        SDL_SetRenderDrawColor(renderer, legalMoveColor.r, legalMoveColor.g,
                               legalMoveColor.b, 180);
        SDL_RenderFillRect(renderer, &toRect);
      }
    }
  }
}

void Window::drawPieces(const Position& pos) {
  for (int rank = 0; rank < 8; ++rank) {
    for (int file = 0; file < 8; ++file) {
      Square sq = makeSquare(file, 7 - rank);
      Piece pc = pos.pieceAt(sq);

      if (pc != NO_PIECE) {
        drawPiece(pc, sq);
      }
    }
  }
}

void Window::drawPiece(Piece pc, Square sq) {
  if (!piecesTexture) return;

  // Get the sprite rect for this piece
  SDL_Rect srcRect = getPieceSpriteRect(pc);

  // Calculate destination position (centered on square)
  int file = fileOf(sq);
  int rank = 7 - rankOf(sq);  // Flip for display

  int padding = squareSize / 20;  // Reduced padding - pieces are larger now
  SDL_Rect dstRect;
  dstRect.x = file * squareSize + padding;
  dstRect.y = rank * squareSize + padding;
  dstRect.w = squareSize - 2 * padding;
  dstRect.h = squareSize - 2 * padding;

  SDL_RenderCopy(renderer, piecesTexture, &srcRect, &dstRect);
}

SDL_Rect Window::getPieceSpriteRect(Piece pc) const {
  PieceType pt = typeOf(pc);
  Color c = colorOf(pc);

  SDL_Rect rect;
  rect.w = pieceWidth;
  rect.h = pieceHeight;

  // Sprite sheet layout: King, Queen, Bishop, Knight, Rook, Pawn
  // Top row is white, bottom row is black
  int col = 0;
  switch (pt) {
    case KING:
      col = 0;
      break;
    case QUEEN:
      col = 1;
      break;
    case BISHOP:
      col = 2;
      break;
    case KNIGHT:
      col = 3;
      break;
    case ROOK:
      col = 4;
      break;
    case PAWN:
      col = 5;
      break;
    default:
      col = 0;
      break;
  }

  rect.x = col * pieceWidth;
  rect.y = (c == WHITE) ? 0 : pieceHeight;

  return rect;
}

void Window::handleClick(int x, int y, Game& game) {
  if (game.isGameOver()) return;

  Square clickedSquare = pixelToSquare(x, y);
  if (clickedSquare == NO_SQUARE) return;

  const Position& pos = game.getPosition();

  if (!pieceSelected) {
    // Select a piece
    Piece pc = pos.pieceAt(clickedSquare);
    if (pc != NO_PIECE && colorOf(pc) == pos.sideToMove()) {
      selectedSquare = clickedSquare;
      pieceSelected = true;
      std::cout << "Selected square: " << squareToString(clickedSquare) << "\n";
    }
  } else {
    // Try to make a move
    if (clickedSquare == selectedSquare) {
      // Deselect
      pieceSelected = false;
      selectedSquare = NO_SQUARE;
    } else {
      // Try all possible move types
      Move move = makeMove(selectedSquare, clickedSquare);

      // Check if it's a legal move
      bool moveMade = game.makeMove(move);

      if (!moveMade) {
        // Try other move types (promotion, castling, etc.)
        Piece pc = pos.pieceAt(selectedSquare);
        PieceType pt = typeOf(pc);

        // Try promotion
        if (pt == PAWN && (rankOf(clickedSquare) == RANK_8 ||
                           rankOf(clickedSquare) == RANK_1)) {
          move = makePromotion(selectedSquare, clickedSquare, QUEEN);
          moveMade = game.makeMove(move);
        }

        // Try castling
        if (!moveMade && pt == KING &&
            std::abs(selectedSquare - clickedSquare) == 2) {
          move = makeCastling(selectedSquare, clickedSquare);
          moveMade = game.makeMove(move);
        }

        // Try en passant
        if (!moveMade && pt == PAWN && clickedSquare == pos.enPassantSquare()) {
          move = makeEnPassant(selectedSquare, clickedSquare);
          moveMade = game.makeMove(move);
        }
      }

      if (moveMade) {
        std::cout << "Move: " << squareToString(selectedSquare)
                  << squareToString(clickedSquare) << "\n";
        pos.print();

        if (game.isGameOver()) {
          std::cout << "Game over: " << game.getResultString() << "\n";
        }
      } else {
        std::cout << "Illegal move!\n";
      }

      // Reset selection
      pieceSelected = false;
      selectedSquare = NO_SQUARE;
    }
  }
}

Square Window::pixelToSquare(int x, int y) const {
  int file = x / squareSize;
  int rank = 7 - (y / squareSize);  // Flip for display

  if (file < 0 || file > 7 || rank < 0 || rank > 7) return NO_SQUARE;

  return makeSquare(file, rank);
}

void Window::drawAIThinking() {
  if (currentAIMove == 0) return;

  Square from = fromSquare(currentAIMove);
  Square to = toSquare(currentAIMove);

  // Highlight the move being considered with a pulsing effect
  SDL_Color aiMoveColor = {255, 100, 255, 150};  // Purple/magenta

  // Draw "from" square
  int fromFile = fileOf(from);
  int fromRank = 7 - rankOf(from);
  SDL_Rect fromRect;
  fromRect.x = fromFile * squareSize;
  fromRect.y = fromRank * squareSize;
  fromRect.w = squareSize;
  fromRect.h = squareSize;
  SDL_SetRenderDrawColor(renderer, aiMoveColor.r, aiMoveColor.g, aiMoveColor.b,
                         100);
  SDL_RenderFillRect(renderer, &fromRect);

  // Draw "to" square
  int toFile = fileOf(to);
  int toRank = 7 - rankOf(to);
  SDL_Rect toRect;
  toRect.x = toFile * squareSize;
  toRect.y = toRank * squareSize;
  toRect.w = squareSize;
  toRect.h = squareSize;
  SDL_SetRenderDrawColor(renderer, aiMoveColor.r, aiMoveColor.g, aiMoveColor.b,
                         150);
  SDL_RenderFillRect(renderer, &toRect);

  // Draw arrow or line from -> to
  SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255);
  int fromX = fromFile * squareSize + squareSize / 2;
  int fromY = fromRank * squareSize + squareSize / 2;
  int toX = toFile * squareSize + squareSize / 2;
  int toY = toRank * squareSize + squareSize / 2;

  // Draw thick line (simulate by drawing multiple lines)
  for (int offset = -2; offset <= 2; ++offset) {
    SDL_RenderDrawLine(renderer, fromX + offset, fromY, toX + offset, toY);
    SDL_RenderDrawLine(renderer, fromX, fromY + offset, toX, toY + offset);
  }
}
