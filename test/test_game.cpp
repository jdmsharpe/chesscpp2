#include "Bitboard.h"
#include "Game.h"
#include "Magic.h"
#include "MoveGen.h"
#include "Zobrist.h"

#include <gtest/gtest.h>

class GameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BB::init();
    Magic::init();
    Zobrist::init();
  }
};

// =============================================================================
// Game Initialization Tests
// =============================================================================

TEST_F(GameTest, DefaultConstruction_StartsFromInitialPosition) {
  Game game;
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);
  EXPECT_FALSE(game.isGameOver());
  EXPECT_EQ(game.getMode(), Game::HUMAN_VS_HUMAN);
  EXPECT_EQ(game.saveFEN(), STARTING_FEN);
}

TEST_F(GameTest, Construction_HumanVsAI) {
  Game game(Game::HUMAN_VS_AI);
  EXPECT_EQ(game.getMode(), Game::HUMAN_VS_AI);
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);
}

TEST_F(GameTest, Construction_AIVsAI) {
  Game game(Game::AI_VS_AI);
  EXPECT_EQ(game.getMode(), Game::AI_VS_AI);
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);
}

TEST_F(GameTest, Reset_RestoresInitialPosition) {
  Game game;
  // Make a move to change the position
  game.makeMove("e2e4");
  EXPECT_NE(game.saveFEN(), STARTING_FEN);

  game.reset();
  EXPECT_EQ(game.saveFEN(), STARTING_FEN);
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);
  EXPECT_FALSE(game.isGameOver());
}

// =============================================================================
// Move Making Tests
// =============================================================================

TEST_F(GameTest, MakeMove_LegalMoveString_ReturnsTrue) {
  Game game;
  EXPECT_TRUE(game.makeMove("e2e4"));
}

TEST_F(GameTest, MakeMove_LegalMoveString_UpdatesPosition) {
  Game game;
  game.makeMove("e2e4");
  // After e2e4, e2 should be empty and e4 should have a white pawn
  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(E2), NO_PIECE);
  EXPECT_EQ(pos.pieceAt(E4), W_PAWN);
  EXPECT_EQ(pos.sideToMove(), BLACK);
}

TEST_F(GameTest, MakeMove_IllegalMoveString_ReturnsFalse) {
  Game game;
  // e2e5 is not a legal pawn move from the starting position
  EXPECT_FALSE(game.makeMove("e2e5"));
}

TEST_F(GameTest, MakeMove_TooShortString_ReturnsFalse) {
  Game game;
  EXPECT_FALSE(game.makeMove("e2"));
}

TEST_F(GameTest, MakeMove_InvalidSquareString_ReturnsFalse) {
  Game game;
  EXPECT_FALSE(game.makeMove("z9z9"));
}

TEST_F(GameTest, MakeMove_EmptySquare_ReturnsFalse) {
  Game game;
  // e3 is empty at the start
  EXPECT_FALSE(game.makeMove("e3e4"));
}

TEST_F(GameTest, MakeMove_LegalMoveObject_ReturnsTrue) {
  Game game;
  Move e2e4 = makeMove(E2, E4);
  EXPECT_TRUE(game.makeMove(e2e4));
}

TEST_F(GameTest, MakeMove_IllegalMoveObject_ReturnsFalse) {
  Game game;
  // Moving a black piece when it's white's turn
  Move e7e5 = makeMove(E7, E5);
  EXPECT_FALSE(game.makeMove(e7e5));
}

// =============================================================================
// Side To Move Alternation Tests
// =============================================================================

TEST_F(GameTest, SideToMove_AlternatesAfterMoves) {
  Game game;
  EXPECT_EQ(game.getPosition().sideToMove(), WHITE);
  game.makeMove("e2e4");
  EXPECT_EQ(game.getPosition().sideToMove(), BLACK);
  game.makeMove("e7e5");
  EXPECT_EQ(game.getPosition().sideToMove(), WHITE);
}

// =============================================================================
// Parse Move Tests
// =============================================================================

TEST_F(GameTest, ParseMove_NormalMove) {
  Game game;
  Move m = game.parseMove("e2e4");
  EXPECT_NE(m, 0);
  EXPECT_EQ(fromSquare(m), E2);
  EXPECT_EQ(toSquare(m), E4);
}

TEST_F(GameTest, ParseMove_PromotionQueen) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  Move m = game.parseMove("a7a8q");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), PROMOTION);
  EXPECT_EQ(promotionType(m), QUEEN);
}

TEST_F(GameTest, ParseMove_PromotionKnight) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  Move m = game.parseMove("a7a8n");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), PROMOTION);
  EXPECT_EQ(promotionType(m), KNIGHT);
}

TEST_F(GameTest, ParseMove_PromotionBishop) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  Move m = game.parseMove("a7a8b");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), PROMOTION);
  EXPECT_EQ(promotionType(m), BISHOP);
}

TEST_F(GameTest, ParseMove_PromotionRook) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  Move m = game.parseMove("a7a8r");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), PROMOTION);
  EXPECT_EQ(promotionType(m), ROOK);
}

TEST_F(GameTest, ParseMove_InvalidPromotionChar_ReturnsZero) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  Move m = game.parseMove("a7a8x");
  EXPECT_EQ(m, 0);
}

TEST_F(GameTest, ParseMove_CastlingKingside) {
  Game game;
  game.loadFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  Move m = game.parseMove("e1g1");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), CASTLING);
}

TEST_F(GameTest, ParseMove_CastlingQueenside) {
  Game game;
  game.loadFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  Move m = game.parseMove("e1c1");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), CASTLING);
}

TEST_F(GameTest, ParseMove_EnPassant) {
  Game game;
  game.loadFEN("8/8/8/pP6/8/8/8/4K2k w - a6 0 1");
  Move m = game.parseMove("b5a6");
  EXPECT_NE(m, 0);
  EXPECT_EQ(moveType(m), EN_PASSANT);
}

TEST_F(GameTest, ParseMove_TooShort_ReturnsZero) {
  Game game;
  EXPECT_EQ(game.parseMove("e2"), 0);
  EXPECT_EQ(game.parseMove(""), 0);
  EXPECT_EQ(game.parseMove("a"), 0);
}

// =============================================================================
// FEN Load / Save Tests
// =============================================================================

TEST_F(GameTest, LoadFEN_ValidFEN_ReturnsTrue) {
  Game game;
  EXPECT_TRUE(game.loadFEN("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2"));
}

TEST_F(GameTest, LoadFEN_UpdatesResult) {
  Game game;
  // Load a checkmate position: Black is checkmated
  // Fool's mate position after the checkmate
  game.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1");
  // After loadFEN, updateGameResult should detect checkmate
  // White is in check and has no legal moves = checkmate (black wins)
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::BLACK_WINS);
}

TEST_F(GameTest, SaveFEN_ReturnsCurrentFEN) {
  Game game;
  std::string fen = game.saveFEN();
  EXPECT_EQ(fen, STARTING_FEN);
}

TEST_F(GameTest, LoadFEN_ThenSaveFEN_Roundtrip) {
  Game game;
  std::string fen = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2";
  game.loadFEN(fen);
  EXPECT_EQ(game.saveFEN(), fen);
}

// =============================================================================
// File Load / Save Tests
// =============================================================================

TEST_F(GameTest, SaveAndLoadFromFile) {
  Game game;
  game.makeMove("e2e4");
  game.makeMove("e7e5");

  std::string expectedFEN = game.saveFEN();

  ASSERT_TRUE(game.saveToFile("/tmp/test_game_save.fen"));

  Game game2;
  ASSERT_TRUE(game2.loadFromFile("/tmp/test_game_save.fen"));
  EXPECT_EQ(game2.saveFEN(), expectedFEN);
}

TEST_F(GameTest, LoadFromFile_NonExistent_ReturnsFalse) {
  Game game;
  EXPECT_FALSE(game.loadFromFile("/tmp/nonexistent_chess_game_12345.fen"));
}

TEST_F(GameTest, SaveToFile_InvalidPath_ReturnsFalse) {
  Game game;
  EXPECT_FALSE(game.saveToFile("/nonexistent_dir/nonexistent_subdir/test.fen"));
}

// =============================================================================
// Game Result String Tests
// =============================================================================

TEST_F(GameTest, GetResultString_InProgress) {
  Game game;
  EXPECT_EQ(game.getResultString(), "Game in progress");
}

TEST_F(GameTest, GetResultString_Draw) {
  Game game;
  // K vs K is a draw by insufficient material
  game.loadFEN("8/8/8/4k3/8/8/8/4K3 w - - 0 1");
  EXPECT_EQ(game.getResultString(), "Draw");
}

TEST_F(GameTest, GetResultString_WhiteWins) {
  Game game;
  // Black is in checkmate: white wins
  // Back rank mate: black king on h8, white rook on a8, white king on g6
  game.loadFEN("R6k/6pp/6K1/8/8/8/8/8 b - - 0 1");
  // Black is in check, no legal moves => checkmate => white wins
  EXPECT_EQ(game.getResult(), Game::WHITE_WINS);
  EXPECT_EQ(game.getResultString(), "White wins");
}

TEST_F(GameTest, GetResultString_BlackWins) {
  Game game;
  // Fool's mate: white is in checkmate
  game.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1");
  EXPECT_EQ(game.getResult(), Game::BLACK_WINS);
  EXPECT_EQ(game.getResultString(), "Black wins");
}

// =============================================================================
// Checkmate Detection Tests
// =============================================================================

TEST_F(GameTest, Checkmate_FoolsMate) {
  Game game;
  // Play the fool's mate sequence
  ASSERT_TRUE(game.makeMove("f2f3"));
  ASSERT_TRUE(game.makeMove("e7e5"));
  ASSERT_TRUE(game.makeMove("g2g4"));
  ASSERT_TRUE(game.makeMove("d8h4"));  // Qh4# checkmate

  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::BLACK_WINS);
}

TEST_F(GameTest, Checkmate_ScholarsMate) {
  Game game;
  // Scholar's mate: 1. e4 e5 2. Bc4 Nc6 3. Qh5 Nf6 4. Qxf7#
  ASSERT_TRUE(game.makeMove("e2e4"));
  ASSERT_TRUE(game.makeMove("e7e5"));
  ASSERT_TRUE(game.makeMove("f1c4"));
  ASSERT_TRUE(game.makeMove("b8c6"));
  ASSERT_TRUE(game.makeMove("d1h5"));
  ASSERT_TRUE(game.makeMove("g8f6"));
  ASSERT_TRUE(game.makeMove("h5f7"));  // Qxf7# checkmate

  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::WHITE_WINS);
}

// =============================================================================
// Stalemate Detection Tests
// =============================================================================

TEST_F(GameTest, Stalemate_Detected) {
  Game game;
  // Classic stalemate: black king in corner, no legal moves but not in check
  // White king on f6, white queen on g6, black king on h8
  game.loadFEN("7k/8/5KQ1/8/8/8/8/8 b - - 0 1");
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

// =============================================================================
// Draw Detection Tests (via Game's updateGameResult)
// =============================================================================

TEST_F(GameTest, Draw_InsufficientMaterial_KingVsKing) {
  Game game;
  game.loadFEN("8/8/8/4k3/8/8/8/4K3 w - - 0 1");
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

TEST_F(GameTest, Draw_InsufficientMaterial_KingBishopVsKing) {
  Game game;
  game.loadFEN("8/8/8/4k3/8/8/8/4KB2 w - - 0 1");
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

TEST_F(GameTest, Draw_InsufficientMaterial_KingKnightVsKing) {
  Game game;
  game.loadFEN("8/8/8/4k3/8/8/8/4KN2 w - - 0 1");
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

TEST_F(GameTest, Draw_FiftyMoveRule) {
  Game game;
  // Halfmove clock at 100 = 50-move rule draw
  game.loadFEN("8/8/8/4k3/8/8/4P3/4K3 w - - 100 1");
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

TEST_F(GameTest, Draw_ThreefoldRepetition) {
  Game game;
  // Use a position with sufficient material and no castling rights
  // so the only draw trigger is threefold repetition.
  // Rook endgame: each side has a king and rook.
  game.loadFEN("4k2r/8/8/8/8/8/8/4K2R w - - 0 1");
  ASSERT_FALSE(game.isGameOver());

  // Cycle 1: Ke2 Ke7 Ke1 Ke8 (back to start - 2nd occurrence)
  ASSERT_TRUE(game.makeMove("e1e2"));
  ASSERT_TRUE(game.makeMove("e8e7"));
  ASSERT_TRUE(game.makeMove("e2e1"));
  ASSERT_TRUE(game.makeMove("e7e8"));
  EXPECT_FALSE(game.isGameOver());  // Only 2 occurrences

  // Cycle 2: Ke2 Ke7 Ke1 Ke8 (back to start - 3rd occurrence)
  ASSERT_TRUE(game.makeMove("e1e2"));
  ASSERT_TRUE(game.makeMove("e8e7"));
  ASSERT_TRUE(game.makeMove("e2e1"));
  ASSERT_TRUE(game.makeMove("e7e8"));
  EXPECT_TRUE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::DRAW);
}

TEST_F(GameTest, NotDraw_SufficientMaterial) {
  Game game;
  // KR vs K - sufficient material
  game.loadFEN("8/8/8/4k3/8/8/8/4KR2 w - - 0 1");
  EXPECT_FALSE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);
}

// =============================================================================
// Move Rejection After Game Over
// =============================================================================

TEST_F(GameTest, MoveAfterCheckmate_IsRejected) {
  Game game;
  // Load a checkmate position
  game.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1");
  ASSERT_TRUE(game.isGameOver());

  // Trying to make any move should fail since there are no legal moves
  EXPECT_FALSE(game.makeMove("e2e4"));
}

// =============================================================================
// Sequence of Moves Tests
// =============================================================================

TEST_F(GameTest, ItalianGameOpening) {
  Game game;
  // 1. e4 e5 2. Nf3 Nc6 3. Bc4
  ASSERT_TRUE(game.makeMove("e2e4"));
  ASSERT_TRUE(game.makeMove("e7e5"));
  ASSERT_TRUE(game.makeMove("g1f3"));
  ASSERT_TRUE(game.makeMove("b8c6"));
  ASSERT_TRUE(game.makeMove("f1c4"));

  EXPECT_FALSE(game.isGameOver());
  EXPECT_EQ(game.getResult(), Game::IN_PROGRESS);

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.sideToMove(), BLACK);
  EXPECT_EQ(pos.pieceAt(E4), W_PAWN);
  EXPECT_EQ(pos.pieceAt(E5), B_PAWN);
  EXPECT_EQ(pos.pieceAt(F3), W_KNIGHT);
  EXPECT_EQ(pos.pieceAt(C6), B_KNIGHT);
  EXPECT_EQ(pos.pieceAt(C4), W_BISHOP);
}

// =============================================================================
// Castling Through Game Interface
// =============================================================================

TEST_F(GameTest, CastlingKingside_ThroughMakeMove) {
  Game game;
  game.loadFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  ASSERT_TRUE(game.makeMove("e1g1"));  // White kingside castling

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(G1), W_KING);
  EXPECT_EQ(pos.pieceAt(F1), W_ROOK);
  EXPECT_EQ(pos.pieceAt(E1), NO_PIECE);
  EXPECT_EQ(pos.pieceAt(H1), NO_PIECE);
}

TEST_F(GameTest, CastlingQueenside_ThroughMakeMove) {
  Game game;
  game.loadFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  ASSERT_TRUE(game.makeMove("e1c1"));  // White queenside castling

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(C1), W_KING);
  EXPECT_EQ(pos.pieceAt(D1), W_ROOK);
  EXPECT_EQ(pos.pieceAt(E1), NO_PIECE);
  EXPECT_EQ(pos.pieceAt(A1), NO_PIECE);
}

// =============================================================================
// En Passant Through Game Interface
// =============================================================================

TEST_F(GameTest, EnPassant_ThroughMakeMove) {
  Game game;
  game.loadFEN("8/8/8/pP6/8/8/8/4K2k w - a6 0 1");
  ASSERT_TRUE(game.makeMove("b5a6"));  // En passant capture

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(A6), W_PAWN);
  EXPECT_EQ(pos.pieceAt(B5), NO_PIECE);
  EXPECT_EQ(pos.pieceAt(A5), NO_PIECE);  // Captured pawn removed
}

// =============================================================================
// Promotion Through Game Interface
// =============================================================================

TEST_F(GameTest, Promotion_ThroughMakeMove) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  ASSERT_TRUE(game.makeMove("a7a8q"));  // Promote to queen

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(A8), W_QUEEN);
  EXPECT_EQ(pos.pieceAt(A7), NO_PIECE);
}

TEST_F(GameTest, PromotionKnight_ThroughMakeMove) {
  Game game;
  game.loadFEN("8/P7/8/8/8/8/8/4K2k w - - 0 1");
  ASSERT_TRUE(game.makeMove("a7a8n"));  // Promote to knight

  const Position& pos = game.getPosition();
  EXPECT_EQ(pos.pieceAt(A8), W_KNIGHT);
}

// =============================================================================
// AI Depth Setting
// =============================================================================

TEST_F(GameTest, SetAIDepth_DoesNotCrash) {
  Game game(Game::HUMAN_VS_AI);
  game.setAIDepth(4);
  // Just verify no crash - we can't easily verify the depth was set
  // without exposing AI internals
  EXPECT_EQ(game.getMode(), Game::HUMAN_VS_AI);
}
