# AI.cpp Refactor + Performance Quick Wins — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor AI.cpp by extracting evaluation into Eval.h/cpp, decomposing negamax() into 5 helpers, adding efficient checking move generation, caching SEE values, and cleaning up minor inconsistencies.

**Architecture:** Extract ~540 lines of evaluation code into a new `Eval` namespace. Decompose the 299-line `negamax()` into ~85 lines of control flow + 5 private helper methods. Add `MoveGen::generateCheckingMoves()` for quiescence optimization. Introduce `ScoredMove` struct to cache SEE computations.

**Tech Stack:** C++20, CMake 3.16+, Google Test, magic bitboards

**Spec:** `docs/superpowers/specs/2026-03-23-ai-refactor-and-perf-design.md`

**Verification after every task:** From `build/` directory:
```bash
cmake --build . && ctest && bash ../scripts/verify_perft.sh
```
Expected perft: depth 1=20, 2=400, 3=8902, 4=197281, 5=4865609.

---

### Task 1: Extract Evaluation into `Eval.h/cpp`

**Files:**
- Create: `inc/Eval.h`
- Create: `src/Eval.cpp`
- Modify: `inc/AI.h:122-147` (remove evaluate declarations)
- Modify: `src/AI.cpp:1-14` (update includes), `src/AI.cpp:391,401,619` (call sites), `src/AI.cpp:702-1252` (delete eval code)
- Modify: `CMakeLists.txt:29-42` (add Eval.cpp to SOURCES)

- [ ] **Step 1: Create `inc/Eval.h`**

```cpp
#pragma once

#include "Position.h"

namespace Eval {
// Evaluate position from the perspective of the side to move.
// Positive = side to move is better. Units: centipawns.
int evaluate(const Position& pos);
}  // namespace Eval
```

- [ ] **Step 2: Create `src/Eval.cpp`**

Copy the following from `src/AI.cpp` into `src/Eval.cpp`:
- Lines 702-759: The entire `namespace PST { ... }` block → wrap in anonymous namespace instead
- Lines 761-861: `AI::evaluate()` → rename to `int evaluate(const Position& pos)` inside `namespace Eval`
- Lines 863-950: `AI::evaluatePawnStructure()` → rename to `static int evaluatePawnStructure(const Position& pos, Color c)`
- Lines 952-985: `AI::evaluateKingSafety()` → same pattern
- Lines 987-1028: `AI::evaluateMobility()` → same pattern (note: drop `const` mismatch — this was non-const in AI)
- Lines 1030-1053: `AI::getGamePhase()` → same pattern
- Lines 1055-1133: `AI::evaluateDevelopment()` → same pattern
- Lines 1135-1166: `AI::evaluateRooks()` → same pattern
- Lines 1168-1178: `AI::evaluateBishops()` → same pattern
- Lines 1180-1252: `AI::evaluateKnights()` → same pattern

File structure:
```cpp
#include "Eval.h"

#include "Bitboard.h"
#include "Magic.h"
#include "Types.h"

namespace {
// Piece-square tables (formerly namespace PST)
constexpr int pawn[64] = { /* exact values from AI.cpp lines 706-714 */ };
constexpr int knight[64] = { /* lines 717-725 */ };
constexpr int bishop[64] = { /* lines 728-736 */ };
constexpr int rook[64] = { /* lines 739-747 */ };
constexpr int kingMiddle[64] = { /* lines 750-758 */ };

// All helper functions as static free functions
static int evaluatePawnStructure(const Position& pos, Color c) { /* lines 863-950 body */ }
static int evaluateKingSafety(const Position& pos, Color c) { /* lines 952-985 body */ }
static int evaluateMobility(const Position& pos, Color c) { /* lines 987-1028 body */ }
static int getGamePhase(const Position& pos) { /* lines 1030-1053 body */ }
static int evaluateDevelopment(const Position& pos, Color c) { /* lines 1055-1133 body */ }
static int evaluateRooks(const Position& pos, Color c) { /* lines 1135-1166 body */ }
static int evaluateBishops(const Position& pos, Color c) { /* lines 1168-1178 body */ }
static int evaluateKnights(const Position& pos, Color c) { /* lines 1180-1252 body */ }
}  // anonymous namespace

namespace Eval {
int evaluate(const Position& pos) {
  // Exact copy of AI::evaluate() body (lines 762-861)
  // but referencing anonymous namespace helpers/PST tables directly
  // (no PST:: prefix needed, no AI:: prefix needed)
}
}  // namespace Eval
```

Key changes when copying:
- `PST::pawn[sq]` → `pawn[sq]` (anonymous namespace, no prefix)
- `evaluatePawnStructure(pos, WHITE)` → `evaluatePawnStructure(pos, WHITE)` (same, just no `this->`)
- `pos.sideToMove() == WHITE ? score : -score` at end — keep as-is

- [ ] **Step 3: Update `CMakeLists.txt` — add `src/Eval.cpp` to SOURCES**

Add `src/Eval.cpp` after `src/Bitboard.cpp` in the SOURCES list (line 31).

Also add `inc/Eval.h` after `inc/Bitboard.h` in the HEADERS list (line 47).

- [ ] **Step 4: Update `src/AI.cpp` — switch to `Eval::evaluate()`**

In `src/AI.cpp`:
1. Add `#include "Eval.h"` after `#include "AI.h"` (line 1)
2. Remove `#include "Magic.h"` (line 13 — AI.cpp no longer needs it directly)
3. Replace 3 call sites:
   - Line 391: `int eval = evaluate(pos);` → `int eval = Eval::evaluate(pos);`
   - Line 401: `int eval = evaluate(pos);` → `int eval = Eval::evaluate(pos);`
   - Line 619: `standPat = evaluate(pos);` → `standPat = Eval::evaluate(pos);`
4. Delete lines 702-1252 (the entire PST namespace, `evaluate()`, and all 8 helper functions)

- [ ] **Step 5: Update `inc/AI.h` — remove eval declarations**

Remove these lines from the `private` section of class `AI`:

```cpp
  // Position evaluation
  int evaluate(const Position& pos);

  // Evaluation helper functions
  int evaluatePawnStructure(const Position& pos, Color c) const;
  int evaluateKingSafety(const Position& pos, Color c) const;
  int evaluateMobility(const Position& pos, Color c);
  int evaluateDevelopment(const Position& pos, Color c) const;
  int evaluateRooks(const Position& pos, Color c) const;
  int evaluateBishops(const Position& pos, Color c) const;
  int evaluateKnights(const Position& pos, Color c) const;
  int getGamePhase(const Position& pos) const;
```

(Lines 122-128 and 139-147 in the current AI.h)

- [ ] **Step 6: Build and verify**

```bash
cd build && cmake .. && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

Expected: All tests pass, perft counts identical.

- [ ] **Step 7: Commit**

```bash
git add inc/Eval.h src/Eval.cpp inc/AI.h src/AI.cpp CMakeLists.txt
git commit -m "refactor: extract evaluation into Eval.h/cpp

Move PST tables, evaluate(), and 8 evaluation helper functions
(~540 lines) from AI.cpp to Eval namespace. AI.cpp now calls
Eval::evaluate(pos) at 3 call sites. No behavior change."
```

---

### Task 2: Decompose `negamax()` — Extract `probeTT` and `storeTT`

**Files:**
- Modify: `inc/AI.h` (add `#include <optional>`, add helper declarations)
- Modify: `src/AI.cpp` (extract TT helpers from negamax)

- [ ] **Step 1: Add declarations to `inc/AI.h`**

Add `#include <optional>` at the top of AI.h with the other includes.

Add to the `private` section of class `AI`, after the existing `isKiller` declaration:

```cpp
  // Search helper: probe transposition table
  // Returns score if TT produces a cutoff, nullopt otherwise.
  // Sets ttMove if the position is found.
  // Note: alpha/beta passed by reference (spec deviation — spec says by-value,
  // but the original code modifies alpha/beta for LOWERBOUND/UPPERBOUND entries
  // and those tightened bounds must persist in the caller).
  std::optional<int> probeTT(HashKey hash, int depth, int& alpha, int& beta, Move& ttMove);

  // Search helper: store result in transposition table
  // Note: takes both alphaOrig and beta (spec deviation — spec omits beta,
  // but both are needed to determine EXACT vs LOWER/UPPER bound flag).
  void storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta);
```

- [ ] **Step 2: Implement `probeTT` in `src/AI.cpp`**

Add after the `isKiller()` implementation (around line 349, after step 1 of task 1 — line numbers will have shifted after eval extraction):

```cpp
std::optional<int> AI::probeTT(HashKey hash, int depth, int& alpha, int& beta, Move& ttMove) {
  size_t ttIndex = hash % TT_SIZE;
  TTEntry& ttEntry = transpositionTable[ttIndex];

  if (ttEntry.key == hash) {
    ttMove = ttEntry.bestMove;

    if (ttEntry.depth >= depth) {
      ttHits++;
      if (ttEntry.flag == EXACT) {
        return ttEntry.score;
      } else if (ttEntry.flag == LOWERBOUND) {
        alpha = std::max(alpha, ttEntry.score);
      } else if (ttEntry.flag == UPPERBOUND) {
        beta = std::min(beta, ttEntry.score);
      }

      if (alpha >= beta) {
        return ttEntry.score;
      }
    }
  }

  return std::nullopt;
}
```

- [ ] **Step 3: Implement `storeTT` in `src/AI.cpp`**

Add right after `probeTT`. Note: the spec's signature has 5 parameters, but the actual code (lines 598-604) needs `beta` to determine the TT flag. The correct signature takes 6 parameters (spec deviation — `beta` was missing from spec):

```cpp
void AI::storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta) {
  size_t ttIndex = hash % TT_SIZE;
  TTEntry& entry = transpositionTable[ttIndex];

  bool shouldReplace = (entry.key == 0) ||
                       (entry.key == hash) ||
                       (entry.depth <= depth) ||
                       (entry.age != ttAge);

  if (shouldReplace) {
    entry.key = hash;
    entry.depth = depth;
    entry.score = score;
    entry.bestMove = bestMove;
    entry.age = ttAge;

    if (score <= alphaOrig) {
      entry.flag = UPPERBOUND;
    } else if (score >= beta) {
      entry.flag = LOWERBOUND;
    } else {
      entry.flag = EXACT;
    }
  }
}
```

Update the declaration in AI.h to match the 6-parameter signature:
```cpp
void storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig, int beta);
```

- [ ] **Step 4: Refactor `negamax()` to use `probeTT` and `storeTT`**

In `negamax()`, replace the TT probe block (lines 318-338 in original, adjusted for eval extraction) with:

```cpp
  int alphaOrig = alpha;
  HashKey hash = pos.hash();
  Move ttMove = 0;

  if (auto score = probeTT(hash, depth, alpha, beta, ttMove)) {
    return *score;
  }
```

And replace the TT store block (lines 583-605 in original) with:

```cpp
  storeTT(hash, depth, maxScore, bestMove, alphaOrig, beta);
```

(Note the 6th argument `beta` — matches the corrected signature from Step 3.)

Also: the IID block that reads `ttEntry` directly (lines 451-453 in original) needs adjustment. Since `probeTT` already set `ttMove` if the position was in the TT, the IID block after the shallow search should re-probe:

```cpp
  if (!ttMove && isPVNode && depth >= 4) {
    int iidDepth = depth - 2;
    (void)negamax(pos, iidDepth, alpha, beta, ply);
    // Re-probe TT to get the move found by IID
    Move iidMove = 0;
    int tmpAlpha = alpha, tmpBeta = beta;
    (void)probeTT(hash, depth, tmpAlpha, tmpBeta, iidMove);
    if (iidMove != 0) {
      // Validate move is in our move list
      bool found = false;
      for (Move m : moves) {
        if (m == iidMove) { found = true; break; }
      }
      if (found) ttMove = iidMove;
    }
  }
```

- [ ] **Step 5: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 6: Commit**

```bash
git add inc/AI.h src/AI.cpp
git commit -m "refactor: extract probeTT and storeTT helpers from negamax

TT probe and store logic (~40 lines) extracted to private helper
methods. negamax() control flow unchanged. No behavior change."
```

---

### Task 3: Decompose `negamax()` — Extract `tryNullMovePruning`

**Files:**
- Modify: `inc/AI.h` (add declaration)
- Modify: `src/AI.cpp` (extract null move logic)

- [ ] **Step 1: Add declaration to `inc/AI.h`**

Add to `private` section:

```cpp
  // Search helper: attempt null move pruning
  // Returns beta if null move causes cutoff, nullopt otherwise.
  std::optional<int> tryNullMovePruning(Position& pos, int depth, int beta, int ply);
```

- [ ] **Step 2: Implement `tryNullMovePruning` in `src/AI.cpp`**

```cpp
std::optional<int> AI::tryNullMovePruning(Position& pos, int depth, int beta, int ply) {
  const int NULL_MOVE_REDUCTION = 3;
  bool canDoNullMove = depth >= 3 && !pos.inCheck() && ply > 0;

  if (canDoNullMove) {
    Color us = pos.sideToMove();
    int material = pos.materialCount(us);

    int knights = BB::popCount(pos.pieces(us, KNIGHT));
    int bishops = BB::popCount(pos.pieces(us, BISHOP));
    int rooks = BB::popCount(pos.pieces(us, ROOK));
    int queens = BB::popCount(pos.pieces(us, QUEEN));
    int nonPawnPieces = knights + bishops + rooks + queens;

    if (material <= 100 || nonPawnPieces == 0 ||
        (nonPawnPieces == 1 && material < 500)) {
      canDoNullMove = false;
    }
  }

  if (canDoNullMove) {
    pos.makeNullMove();
    int nullDepth = std::max(0, depth - 1 - NULL_MOVE_REDUCTION);
    int score = -negamax(pos, nullDepth, -beta, -beta + 1, ply + 1);
    pos.unmakeNullMove();

    if (score >= beta) {
      return beta;
    }
  }

  return std::nullopt;
}
```

- [ ] **Step 3: Refactor `negamax()` to use the helper**

Replace the null move pruning block in `negamax()` with:

```cpp
  if (auto score = tryNullMovePruning(pos, depth, beta, ply)) {
    return *score;
  }
```

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 5: Commit**

```bash
git add inc/AI.h src/AI.cpp
git commit -m "refactor: extract tryNullMovePruning helper from negamax

Null move pruning + zugzwang detection (~36 lines) extracted to
private helper. No behavior change."
```

---

### Task 4: Decompose `negamax()` — Extract `canPrune`

**Files:**
- Modify: `inc/AI.h` (add PruningResult struct + declaration)
- Modify: `src/AI.cpp` (extract pruning logic)

- [ ] **Step 1: Add declarations to `inc/AI.h`**

Add to `private` section:

```cpp
  // Search helper: check static pruning conditions
  struct PruningResult {
    bool cutoff;         // true = return score immediately
    int score;           // only valid if cutoff == true
    bool futilityPrune;  // true = skip quiet moves in move loop
  };
  PruningResult canPrune(Position& pos, int depth, int alpha, int beta, bool isPVNode);
```

- [ ] **Step 2: Implement `canPrune` in `src/AI.cpp`**

```cpp
AI::PruningResult AI::canPrune(Position& pos, int depth, int alpha, int beta, bool isPVNode) {
  PruningResult result = {false, 0, false};

  // All pruning requires not being in check
  if (pos.inCheck()) return result;

  int eval = Eval::evaluate(pos);

  // Reverse Futility Pruning (Static Null Move Pruning)
  // Only at non-PV nodes
  if (depth <= 6 && !isPVNode) {
    int rfpMargin = 100 * depth;
    if (eval - rfpMargin >= beta) {
      result.cutoff = true;
      result.score = eval;
      return result;
    }
  }

  // Razoring — only at non-PV nodes
  if (depth <= 3 && !isPVNode) {
    int razoringMargin = 300 + 150 * depth;
    if (eval + razoringMargin < alpha) {
      int qscore = quiescence(pos, alpha, beta, 0);
      if (qscore < alpha) {
        result.cutoff = true;
        result.score = qscore;
        return result;
      }
    }
  }

  // Futility Pruning flag — applies at PV nodes too (only gated by !inCheck)
  if (depth <= 3) {
    int futilityMargin = 100 + 200 * depth;
    int futilityValue = eval + futilityMargin;
    if (futilityValue <= alpha) {
      result.futilityPrune = true;
    }
  }

  return result;
}
```

- [ ] **Step 3: Refactor `negamax()` to use the helper**

Replace the reverse futility + razoring + futility blocks in `negamax()` with:

```cpp
  bool isPVNode = (beta - alpha) > 1;

  auto pruning = canPrune(pos, depth, alpha, beta, isPVNode);
  if (pruning.cutoff) return pruning.score;
  bool futilityPrune = pruning.futilityPrune;
```

Remove the old `bool futilityPrune = false;` declaration and the three separate pruning blocks.

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 5: Commit**

```bash
git add inc/AI.h src/AI.cpp
git commit -m "refactor: extract canPrune helper from negamax

Reverse futility, razoring, and futility pruning (~35 lines)
extracted to PruningResult-returning helper. Computes eval once
instead of up to 3 times. No behavior change."
```

---

### Task 5: Decompose `negamax()` — Extract `searchMove`

**Files:**
- Modify: `inc/AI.h` (add declaration)
- Modify: `src/AI.cpp` (extract PVS+LMR logic)

- [ ] **Step 1: Add declaration to `inc/AI.h`**

Add to `private` section:

```cpp
  // Search helper: search a single move with PVS + LMR
  // Handles makeMove/unmakeMove internally.
  int searchMove(Position& pos, Move move, int depth, int alpha, int beta,
                 int ply, size_t moveNum, bool isCapture, bool isPromotion);
```

- [ ] **Step 2: Implement `searchMove` in `src/AI.cpp`**

```cpp
int AI::searchMove(Position& pos, Move move, int depth, int alpha, int beta,
                   int ply, size_t moveNum, bool isCapture, bool isPromotion) {
  pos.makeMove(move);

  // Check Extension
  bool givesCheck = pos.inCheck();
  int extension = givesCheck ? 1 : 0;
  int newDepth = depth - 1 + extension;

  int score;

  // Principal Variation Search (PVS)
  if (moveNum == 0) {
    // First move — full window
    score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
  } else {
    // Late Move Reductions (LMR)
    int reduction = 0;

    if (depth >= 3 && moveNum >= 3 && !isCapture && !givesCheck &&
        !isPromotion && !isKiller(move, ply)) {
      reduction = 1;
      if (depth >= 3 && moveNum >= 3) {
        reduction = 1 + (depth >= 6 ? 1 : 0) + (moveNum >= 6 ? 1 : 0);
        if (depth >= 8 && moveNum >= 10) {
          reduction++;
        }
      }
      reduction = std::min(reduction, newDepth);
    }

    // PVS: null window search first
    score = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1);

    // Re-search if null window fails high
    if (score > alpha && score < beta) {
      if (reduction > 0) {
        score = -negamax(pos, newDepth, -alpha - 1, -alpha, ply + 1);
      }
      if (score > alpha && score < beta) {
        score = -negamax(pos, newDepth, -beta, -alpha, ply + 1);
      }
    }
  }

  pos.unmakeMove();
  return score;
}
```

- [ ] **Step 3: Refactor `negamax()` move loop to use `searchMove`**

Replace the move loop body (makeMove through unmakeMove) with:

```cpp
  for (size_t moveNum = 0; moveNum < moves.size(); ++moveNum) {
    Move move = moves[moveNum];

    bool isCapture = pos.pieceAt(toSquare(move)) != NO_PIECE ||
                     moveType(move) == EN_PASSANT;
    bool isPromotion = moveType(move) == PROMOTION;

    // Futility pruning: skip quiet moves if position is hopeless
    if (futilityPrune && moveNum > 0 && !isCapture && !isPromotion) {
      continue;
    }

    // Late Move Pruning
    if (depth <= 3 && moveNum >= static_cast<size_t>(3 + depth * depth) &&
        !isCapture && !isPromotion && !isKiller(move, ply)) {
      continue;
    }

    int score = searchMove(pos, move, depth, alpha, beta, ply, moveNum,
                           isCapture, isPromotion);

    if (score > maxScore) {
      maxScore = score;
      bestMove = move;

      // Update PV
      pvTable[ply][0] = move;
      for (int i = 0; i < pvLength[ply + 1]; ++i) {
        pvTable[ply][i + 1] = pvTable[ply + 1][i];
      }
      pvLength[ply] = pvLength[ply + 1] + 1;
    }

    alpha = std::max(alpha, score);

    if (alpha >= beta) {
      if (!isCapture) {
        storeKiller(move, ply);
        updateHistory(move, depth);
        if (ply > 0 && pvLength[ply - 1] > 0) {
          Move prevMove = pvTable[ply - 1][0];
          countermoves[fromSquare(prevMove)][toSquare(prevMove)] = move;
        }
      }
      break;
    }
  }
```

Note: the `isCapture` redeclaration at the cutoff point (line 565 in original) used to shadow the earlier one. Now both are in the same scope, so we reuse the variable from the loop body.

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 5: Commit**

```bash
git add inc/AI.h src/AI.cpp
git commit -m "refactor: extract searchMove helper from negamax

PVS + LMR + check extension logic (~53 lines) extracted to private
helper. negamax() now ~85 lines of clear control flow. No behavior
change."
```

---

### Task 6: Add `generateCheckingMoves()` to MoveGen

**Files:**
- Modify: `inc/MoveGen.h` (add declaration)
- Modify: `src/MoveGen.cpp` (add implementation)
- Modify: `src/AI.cpp` (update quiescence to use it)

- [ ] **Step 1: Add declaration to `inc/MoveGen.h`**

Add after the `generateCaptures` declaration:

```cpp
// Generate non-capture moves that give check (for quiescence search)
// Uses the fallback approach: generates pseudo-legal non-captures,
// makes each move, checks if it gives check, filters for legality.
std::vector<Move> generateCheckingMoves(Position& pos);
```

- [ ] **Step 2: Implement `generateCheckingMoves` in `src/MoveGen.cpp`**

Add before `perft()`:

```cpp
std::vector<Move> generateCheckingMoves(Position& pos) {
  std::vector<Move> pseudoMoves = generatePseudoLegalMoves(pos);
  std::vector<Move> checks;

  for (Move move : pseudoMoves) {
    Square to = toSquare(move);
    // Skip captures — already handled by generateCaptures
    if (pos.pieceAt(to) != NO_PIECE || moveType(move) == EN_PASSANT) {
      continue;
    }

    // Make the move and check if it gives check
    Color us = pos.sideToMove();
    pos.makeMove(move);

    bool givesCheck = pos.inCheck();
    // Also verify legality: our king must not be in check
    bool legal = !pos.isAttacked(BB::lsb(pos.pieces(us, KING)), pos.sideToMove());

    pos.unmakeMove();

    if (givesCheck && legal) {
      checks.push_back(move);
    }
  }

  return checks;
}
```

This uses the fallback approach from the spec: generate all pseudo-legal non-captures, test each for check. This avoids the complexity of discovered check detection while still being faster than `generateLegalMoves` (we skip the legality check for non-checking moves — ~25 out of ~30 quiet moves).

- [ ] **Step 3: Update quiescence search in `src/AI.cpp`**

Replace the checking move generation block in `quiescence()`. Find the block that starts with `if (qsDepth == 0 && !inCheck)` and generates all legal moves to find checks. Replace with:

```cpp
  // At first qsearch ply, also try checking moves to avoid horizon effect
  if (qsDepth == 0 && !inCheck) {
    std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
    captures.insert(captures.end(), checks.begin(), checks.end());
  }
```

This replaces ~18 lines with 4 lines.

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

Perft must be unchanged (this only affects quiescence, not move generation).

- [ ] **Step 5: Commit**

```bash
git add inc/MoveGen.h src/MoveGen.cpp src/AI.cpp
git commit -m "perf: add generateCheckingMoves for quiescence optimization

New MoveGen function generates only non-capture checking moves
instead of generating all legal moves and filtering. Reduces
quiescence overhead by skipping legality checks on ~25 non-checking
quiet moves per position."
```

---

### Task 7: SEE Caching via `ScoredMove`

**Files:**
- Modify: `inc/Types.h` (add ScoredMove struct)
- Modify: `inc/AI.h` (update orderMoves/getMoveScore signatures)
- Modify: `src/AI.cpp` (plumb ScoredMove through search)

- [ ] **Step 1: Add `ScoredMove` to `inc/Types.h`**

Add at the end of the file, before the closing of the header:

```cpp
// Move with cached scoring data for move ordering
struct ScoredMove {
  Move move;
  int score;      // move ordering score
  int seeValue;   // cached SEE value (INT_MIN = not computed)
};
```

- [ ] **Step 2: Update `orderMoves` signature in `inc/AI.h`**

Change:
```cpp
  void orderMoves(Position& pos, std::vector<Move>& moves, int ply, Move ttMove = 0);
  int getMoveScore(const Position& pos, Move move, int ply, Move ttMove);
```

To:
```cpp
  std::vector<ScoredMove> orderMoves(Position& pos, const std::vector<Move>& moves,
                                     int ply, Move ttMove = 0);
  ScoredMove scoreMoveWithSEE(const Position& pos, Move move, int ply, Move ttMove);
```

- [ ] **Step 3: Implement `scoreMoveWithSEE` in `src/AI.cpp`**

Replace `getMoveScore` with:

```cpp
ScoredMove AI::scoreMoveWithSEE(const Position& pos, Move move, int ply, Move ttMove) {
  ScoredMove sm;
  sm.move = move;
  sm.seeValue = std::numeric_limits<int>::min();  // not computed yet

  // Hash move gets highest priority
  if (move == ttMove) {
    sm.score = 1000000;
    return sm;
  }

  sm.score = 0;
  Square from = fromSquare(move);
  Square to = toSquare(move);
  Piece captured = pos.pieceAt(to);

  if (captured != NO_PIECE || moveType(move) == EN_PASSANT) {
    // Compute and cache SEE
    sm.seeValue = pos.see(move);

    if (sm.seeValue > 0) {
      sm.score = 20000 + sm.seeValue;
    } else if (sm.seeValue == 0) {
      sm.score = 10000;
    } else {
      sm.score = 5000 + sm.seeValue;
    }
  } else {
    // Quiet moves — same as before
    if (ply > 0 && pvLength[ply - 1] > 0) {
      Move prevMove = pvTable[ply - 1][0];
      Move countermove = countermoves[fromSquare(prevMove)][toSquare(prevMove)];
      if (move == countermove) {
        sm.score = 9500;
      }
    }
    if (isKiller(move, ply)) {
      sm.score += 9000;
    }
    sm.score += historyTable[from][to];
  }

  if (moveType(move) == PROMOTION) {
    sm.score += 15000;
  }

  return sm;
}
```

- [ ] **Step 4: Update `orderMoves` to return `vector<ScoredMove>`**

```cpp
std::vector<ScoredMove> AI::orderMoves(Position& pos, const std::vector<Move>& moves,
                                       int ply, Move ttMove) {
  std::vector<ScoredMove> scored;
  scored.reserve(moves.size());
  for (Move m : moves) {
    scored.push_back(scoreMoveWithSEE(pos, m, ply, ttMove));
  }
  std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
    return a.score > b.score;
  });
  return scored;
}
```

- [ ] **Step 5: Update `negamax()` to use `ScoredMove`**

Change the move ordering and loop in `negamax()`:

```cpp
  auto scoredMoves = orderMoves(pos, moves, ply, ttMove);

  int maxScore = std::numeric_limits<int>::min();
  Move bestMove = scoredMoves[0].move;
  pvLength[ply] = 0;

  for (size_t moveNum = 0; moveNum < scoredMoves.size(); ++moveNum) {
    Move move = scoredMoves[moveNum].move;
    // ... rest of loop uses `move` as before
```

- [ ] **Step 6: Update `quiescence()` to use cached SEE**

Replace the inline sort and SEE pruning:

```cpp
  // Build ScoredMove list with cached SEE
  std::vector<ScoredMove> scoredCaptures;
  scoredCaptures.reserve(captures.size());
  for (Move m : captures) {
    scoredCaptures.push_back(scoreMoveWithSEE(pos, m, 0, 0));
  }
  std::sort(scoredCaptures.begin(), scoredCaptures.end(),
            [](const ScoredMove& a, const ScoredMove& b) {
              return a.score > b.score;
            });

  for (const ScoredMove& sm : scoredCaptures) {
    // SEE pruning — use cached value
    if (sm.seeValue != std::numeric_limits<int>::min() && sm.seeValue < 0) {
      continue;
    }

    // Futility pruning
    Square to = toSquare(sm.move);
    Piece captured = pos.pieceAt(to);
    if (captured != NO_PIECE) {
      static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
      if (standPat + pieceValues[typeOf(captured)] + 200 < alpha) {
        continue;
      }
    }

    pos.makeMove(sm.move);
    int score = -quiescence(pos, -beta, -alpha, qsDepth + 1);
    pos.unmakeMove();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }
```

- [ ] **Step 7: Update `findBestMove()` root move ordering**

In `findBestMove()`, the root also calls `orderMoves`. Update it to work with the new return type:

```cpp
    // Order moves based on previous iteration's best move
    auto scoredRootMoves = orderMoves(pos, rootMoves, 0, bestMove);

    // Extract back to plain moves for the root loop
    // (root loop needs to re-order each iteration)
    rootMoves.clear();
    for (const auto& sm : scoredRootMoves) {
      rootMoves.push_back(sm.move);
    }
```

Or alternatively, convert `rootMoves` to `vector<ScoredMove>` throughout `findBestMove()`. The simpler approach is to keep `rootMoves` as `vector<Move>` and just rebuild each iteration as shown above.

- [ ] **Step 8: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 9: Commit**

```bash
git add inc/Types.h inc/AI.h src/AI.cpp
git commit -m "perf: cache SEE values via ScoredMove struct

Introduce ScoredMove to store cached SEE alongside move ordering
score. Eliminates redundant SEE recomputation in quiescence search.
getMoveScore renamed to scoreMoveWithSEE, orderMoves returns
vector<ScoredMove>."
```

---

### Task 8: Minor Cleanups

**Files:**
- Modify: `inc/Polyglot.h` (header guard)
- Modify: `inc/Tablebase.h` (header guard)
- Modify: `inc/Position.h` (remove seeCapture declaration)
- Modify: `src/Position.cpp` (remove seeCapture implementation)

- [ ] **Step 1: Unify header guards — `Polyglot.h`**

Replace the first 3 lines and last line:
```cpp
#ifndef POLYGLOT_H
#define POLYGLOT_H
```
→
```cpp
#pragma once
```

And remove the final `#endif  // POLYGLOT_H` line.

- [ ] **Step 2: Unify header guards — `Tablebase.h`**

Same pattern:
```cpp
#ifndef TABLEBASE_H
#define TABLEBASE_H
```
→
```cpp
#pragma once
```

Remove final `#endif  // TABLEBASE_H`.

- [ ] **Step 3: Remove deprecated `seeCapture` from `Position.h`**

Remove line 71:
```cpp
  int seeCapture(Square to, Color side, Piece captured, Piece attacker) const;
```

- [ ] **Step 4: Remove deprecated `seeCapture` from `Position.cpp`**

Remove lines 685-690:
```cpp
// Recursive SEE - simulate capture sequence (DEPRECATED - use see() instead)
int Position::seeCapture(Square, Color, Piece captured, Piece) const {
  // This function is now unused but kept for backward compatibility
  static const int pieceValues[6] = {100, 320, 330, 500, 900, 20000};
  return pieceValues[typeOf(captured)];
}
```

- [ ] **Step 5: Build and verify**

```bash
cd build && cmake --build . && ctest && bash ../scripts/verify_perft.sh
```

- [ ] **Step 6: Commit**

```bash
git add inc/Polyglot.h inc/Tablebase.h inc/Position.h src/Position.cpp
git commit -m "cleanup: unify header guards to pragma once, remove dead code

Convert Polyglot.h and Tablebase.h from #ifndef guards to #pragma
once (matching all other headers). Remove unused deprecated
seeCapture() method."
```
