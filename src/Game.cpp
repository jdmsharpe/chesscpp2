#include "Game.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "MoveGen.h"

Game::Game(GameMode mode) : mode(mode), result(IN_PROGRESS) { reset(); }

void Game::reset() {
  position = Position();
  result = IN_PROGRESS;
}

bool Game::makeMove(const std::string& moveStr) {
  Move move = parseMove(moveStr);
  if (move == 0) return false;
  return makeMove(move);
}

bool Game::makeMove(Move move) {
  // Check if move is legal
  std::vector<Move> legalMoves = MoveGen::generateLegalMoves(position);
  if (std::find(legalMoves.begin(), legalMoves.end(), move) ==
      legalMoves.end()) {
    return false;
  }

  position.makeMove(move);
  updateGameResult();
  return true;
}

Move Game::parseMove(const std::string& moveStr) {
  if (moveStr.length() < 4) return 0;

  // Parse from/to squares
  Square from = stringToSquare(moveStr.substr(0, 2));
  Square to = stringToSquare(moveStr.substr(2, 2));

  if (from == NO_SQUARE || to == NO_SQUARE) return 0;

  // Check for promotion
  if (moveStr.length() >= 5) {
    char promoPiece = moveStr[4];
    PieceType pt = QUEEN;

    switch (promoPiece) {
      case 'n':
      case 'N':
        pt = KNIGHT;
        break;
      case 'b':
      case 'B':
        pt = BISHOP;
        break;
      case 'r':
      case 'R':
        pt = ROOK;
        break;
      case 'q':
      case 'Q':
        pt = QUEEN;
        break;
      default:
        return 0;
    }

    return makePromotion(from, to, pt);
  }

  // Check if this is a special move
  Piece pc = position.pieceAt(from);
  if (pc == NO_PIECE) return 0;

  PieceType pt = typeOf(pc);

  // Castling
  if (pt == KING && std::abs(from - to) == 2) {
    return makeCastling(from, to);
  }

  // En passant
  if (pt == PAWN && to == position.enPassantSquare()) {
    return makeEnPassant(from, to);
  }

  // Normal move
  return ::makeMove(from, to);  // Use global scope to avoid name conflict
}

Move Game::getAIMove() { return ai.findBestMove(position); }

void Game::updateGameResult() {
  std::vector<Move> legalMoves = MoveGen::generateLegalMoves(position);

  if (legalMoves.empty()) {
    if (position.inCheck()) {
      // Checkmate
      result = (position.sideToMove() == WHITE) ? BLACK_WINS : WHITE_WINS;
    } else {
      // Stalemate
      result = DRAW;
    }
  } else if (position.isDraw()) {
    // 50-move rule, threefold repetition, or insufficient material
    result = DRAW;
  }
}

std::string Game::getResultString() const {
  switch (result) {
    case WHITE_WINS:
      return "White wins";
    case BLACK_WINS:
      return "Black wins";
    case DRAW:
      return "Draw";
    case IN_PROGRESS:
      return "Game in progress";
    default:
      return "Unknown";
  }
}

bool Game::loadFEN(const std::string& fen) {
  if (position.setFromFEN(fen)) {
    updateGameResult();
    return true;
  }
  return false;
}

std::string Game::saveFEN() const { return position.getFEN(); }

bool Game::loadFromFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) return false;

  std::string fen;
  std::getline(file, fen);
  file.close();

  return loadFEN(fen);
}

bool Game::saveToFile(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) return false;

  file << saveFEN() << "\n";
  file.close();

  return true;
}
