# AI.cpp Refactor + Performance Quick Wins

**Date:** 2026-03-23
**Status:** Approved
**Scope:** Code Health (refactor AI.cpp) + Playing Strength (search performance)

## Goals

1. Extract evaluation logic into `Eval.h/cpp` — clear separation of search vs evaluation
2. Decompose `negamax()` from 299 lines into ~85 lines of control flow + 5 named helpers
3. Add `MoveGen::generateCheckingMoves()` to avoid generating all moves in quiescence
4. Cache SEE values via `ScoredMove` struct to eliminate redundant computation
5. Minor cleanups: unify header guards, remove deprecated `seeCapture()`

## Non-Goals

- Lazy SMP multithreading (future work, but this refactor prepares for it)
- Eval tuning or new eval terms
- Opening book or tablebase changes
- Test infrastructure changes

## Verification

After every change: `ctest` + `bash scripts/verify_perft.sh`

Expected perft from startpos: depth 1 = 20, depth 2 = 400, depth 3 = 8,902, depth 4 = 197,281, depth 5 = 4,865,609. Any deviation = regression.

---

## Part 1: Extract Evaluation into `Eval.h/cpp`

### What Moves

| Component | Current Location (AI.cpp) | Lines |
|-----------|--------------------------|-------|
| `PST` namespace (piece-square tables) | 702-759 | ~58 |
| `evaluate()` | 761-861 | ~100 |
| `evaluatePawnStructure()` | 863-950 | ~88 |
| `evaluateKingSafety()` | 952-985 | ~34 |
| `evaluateMobility()` | 987-1028 | ~42 |
| `getGamePhase()` | 1030-1053 | ~24 |
| `evaluateDevelopment()` | 1055-1133 | ~79 |
| `evaluateRooks()` | 1135-1166 | ~32 |
| `evaluateBishops()` | 1168-1178 | ~11 |
| `evaluateKnights()` | 1180-1252 | ~73 |
| **Total** | | **~541** |

### New Files

**`inc/Eval.h`:**
```cpp
#pragma once
#include "Position.h"

namespace Eval {
  // Evaluate position from the perspective of the side to move.
  // Positive = side to move is better. Units: centipawns.
  int evaluate(const Position& pos);
}
```

**`src/Eval.cpp`:**
- `#include "Eval.h"`, `"Bitboard.h"`, `"Magic.h"`, `"Types.h"`
- PST tables in anonymous namespace (not `namespace PST` — no external access needed)
- All `evaluateX()` functions become `static` free functions (not class methods)
- `evaluate()` is the single public entry point in `namespace Eval`

### Changes to AI.h

Remove from `AI` class:
- `int evaluate(const Position& pos);`
- `int evaluatePawnStructure(const Position& pos, Color c) const;`
- `int evaluateKingSafety(const Position& pos, Color c) const;`
- `int evaluateMobility(const Position& pos, Color c);`
- `int evaluateDevelopment(const Position& pos, Color c) const;`
- `int evaluateRooks(const Position& pos, Color c) const;`
- `int evaluateBishops(const Position& pos, Color c) const;`
- `int evaluateKnights(const Position& pos, Color c) const;`
- `int getGamePhase(const Position& pos) const;`

### Changes to AI.cpp

- Add `#include "Eval.h"`
- Remove `#include "Magic.h"` (AI.cpp never calls Magic functions directly — `pos.see()` is a Position method that handles Magic internally, and mobility evaluation is now in Eval.cpp)
- Replace 3 call sites:
  - `negamax()` line 391: `int eval = evaluate(pos);` → `int eval = Eval::evaluate(pos);`
  - `negamax()` line 401: `int eval = evaluate(pos);` → `int eval = Eval::evaluate(pos);`
  - `quiescence()` line 619: `standPat = evaluate(pos);` → `standPat = Eval::evaluate(pos);`
- Delete lines 702-1252 (PST + evaluate + all helpers)

### Build System

Add to `CMakeLists.txt` source list: `src/Eval.cpp`

---

## Part 2: Decompose `negamax()` into Helpers

All helpers are `private` methods on the `AI` class. They don't call each other or recurse — `negamax()` retains all control flow.

### Helper 1: `probeTT`

```cpp
// Returns a score if TT produces a cutoff, nullopt otherwise.
// Sets ttMove to the stored best move if the position is found.
std::optional<int> probeTT(HashKey hash, int depth, int alpha, int beta, Move& ttMove);
```

Encapsulates: TT index calculation, key match, depth check, EXACT/LOWER/UPPER bound logic, alpha-beta cutoff. Currently lines 322-338.

### Helper 2: `storeTT`

```cpp
// Store search result in the transposition table.
void storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig);
```

Encapsulates: replacement policy (empty/same/deeper/older), flag determination (EXACT/LOWER/UPPER). Currently lines 583-605.

### Helper 3: `tryNullMovePruning`

```cpp
// Attempt null move pruning. Returns beta if cutoff, nullopt otherwise.
std::optional<int> tryNullMovePruning(Position& pos, int depth, int beta, int ply);
```

Encapsulates: eligibility check (depth >= 3, not in check, not root), zugzwang detection (material count, non-pawn piece count), null move search with reduction. Currently lines 348-383.

### Helper 4: `canPrune`

```cpp
struct PruningResult {
  bool cutoff;           // true = return score immediately
  int score;             // only valid if cutoff == true
  bool futilityPrune;    // true = skip quiet moves later in the move loop
};

// Check static pruning conditions: reverse futility, razoring, futility.
PruningResult canPrune(Position& pos, int depth, int alpha, int beta, bool isPVNode);
```

Encapsulates three pruning techniques, currently lines 388-422. Calls `Eval::evaluate(pos)` internally, and `quiescence()` for razoring (the razoring path calls qsearch to verify the position is truly hopeless).

**Important gating differences:** The three techniques have different conditions:

- Reverse futility pruning (depth <= 6): gated by `!isPVNode && !pos.inCheck()`
- Razoring (depth <= 3): gated by `!isPVNode && !pos.inCheck()`
- Futility flag (depth <= 3): gated by `!pos.inCheck()` only — **applies at PV nodes too**

The helper must NOT early-exit on `isPVNode` for all three checks. Implementation should:

1. If `pos.inCheck()`, return no cutoff and `futilityPrune = false`
2. Compute `int eval = Eval::evaluate(pos)` once
3. If `!isPVNode`: check reverse futility (can cutoff), then razoring (can cutoff)
4. Regardless of `isPVNode`: check futility flag (sets `futilityPrune`, never a cutoff)

### Helper 5: `searchMove`

```cpp
// Search a single move with PVS + LMR. Returns the score.
int searchMove(Position& pos, Move move, int depth, int alpha, int beta,
               int ply, size_t moveNum, bool isCapture, bool isPromotion);
```

Encapsulates: check extension detection, LMR reduction calculation, PVS null-window search, re-search on fail-high, full-window re-search. Currently lines 493-545.

`searchMove` handles `makeMove`/`unmakeMove` internally — the caller passes the move and gets a score back. This keeps the move loop in `negamax()` clean: no bracket matching to worry about.

### What `negamax()` Becomes

```
negamax(pos, depth, alpha, beta, ply):
  nodesSearched++
  time check (every 1024 nodes)

  int alphaOrig = alpha  // must capture before TT probe modifies alpha
  hash = pos.hash()
  Move ttMove = 0
  if (auto score = probeTT(hash, depth, alpha, beta, ttMove))
    return *score

  if (depth == 0)
    return quiescence(pos, alpha, beta, 0)

  if (auto score = tryNullMovePruning(pos, depth, beta, ply))
    return *score

  bool isPVNode = (beta - alpha) > 1
  auto pruning = canPrune(pos, depth, alpha, beta, isPVNode)
  if (pruning.cutoff) return pruning.score

  moves = generateLegalMoves(pos)
  if (moves.empty()) return checkmate_or_stalemate(pos, ply)

  // IID (unchanged — small block, not worth extracting)
  if (!ttMove && isPVNode && depth >= 4) { ... }

  orderMoves(pos, moves, ply, ttMove)

  int maxScore = MIN, bestMove = moves[0]
  for (moveNum, move in moves):
    // Futility + LMP skip logic (~5 lines, inline)
    score = searchMove(pos, move, depth, alpha, beta, ply, moveNum, ...)
    update maxScore, bestMove, PV
    if (alpha >= beta):
      store killer/history/countermove
      break

  storeTT(hash, depth, maxScore, bestMove, alphaOrig)
  return maxScore
```

Approximately 80-90 lines of clear control flow.

### Changes to AI.h

Add `#include <optional>` to the top of `AI.h` (with the other includes).

Add to `private` section of the `AI` class:

```cpp
std::optional<int> probeTT(HashKey hash, int depth, int alpha, int beta, Move& ttMove);
void storeTT(HashKey hash, int depth, int score, Move bestMove, int alphaOrig);
std::optional<int> tryNullMovePruning(Position& pos, int depth, int beta, int ply);

struct PruningResult {
  bool cutoff;
  int score;
  bool futilityPrune;
};
PruningResult canPrune(Position& pos, int depth, int alpha, int beta, bool isPVNode);

int searchMove(Position& pos, Move move, int depth, int alpha, int beta,
               int ply, size_t moveNum, bool isCapture, bool isPromotion);
```

---

## Part 3: Performance Quick Wins

### 3a. `generateCheckingMoves()` in MoveGen

**Problem:** In `quiescence()` at `qsDepth == 0`, the engine generates ALL legal moves and filters for checks:
```cpp
std::vector<Move> allMoves = MoveGen::generateLegalMoves(pos);
for (Move m : allMoves) {
  if (!isCapture) {
    pos.makeMove(m);
    bool givesCheck = pos.inCheck();
    pos.unmakeMove();
    if (givesCheck) checks.push_back(m);
  }
}
```

**Fix:** Add to `MoveGen`:
```cpp
std::vector<Move> generateCheckingMoves(Position& pos);
```

Implementation must handle both **direct checks** and **discovered checks**:

**Direct checks** (piece moves to a square that attacks the king):

1. Find the enemy king square
2. For each piece type, compute which squares attack the king (e.g., `BB::knightAttacks(kingSq)` gives squares from which a knight checks the king)
3. Intersect with each piece's legal destinations
4. Filter for legality with make/unmake
5. Exclude captures (those are already in the captures list)

**Discovered checks** (moving a piece unblocks a slider's line to the king):

1. Identify pieces on lines between friendly sliders and the enemy king. For each friendly slider (bishop/rook/queen), compute the ray from the slider through the king. If exactly one friendly piece sits on that ray (between slider and king), that piece is a "discovery candidate."
2. Any legal non-capture move by a discovery candidate gives a discovered check. Generate all such moves.
3. These moves are already legal-filtered via make/unmake.

Both the current brute-force code (make every legal move, test `pos.inCheck()`) and this approach must produce the same set of checking moves. The difference is efficiency: instead of generating ~30 quiet moves and testing each, we generate only the ~1-5 that actually give check.

**Fallback option:** If discovered check detection proves too complex, a simpler approach is to generate all pseudo-legal non-capture moves, make each one, test `pos.inCheck()`, and only keep checks — identical to the current approach but operating on pseudo-legal moves (skipping legality check for non-checks). This still avoids the `generateLegalMoves` overhead for the ~25 moves that don't give check.

**Changes to MoveGen.h:**
```cpp
// Generate non-capture moves that give check (for quiescence search)
std::vector<Move> generateCheckingMoves(Position& pos);
```

**Changes to AI.cpp (quiescence):**
Replace the all-moves-then-filter block (lines 644-662) with:
```cpp
if (qsDepth == 0 && !inCheck) {
  std::vector<Move> checks = MoveGen::generateCheckingMoves(pos);
  captures.insert(captures.end(), checks.begin(), checks.end());
}
```

**Expected impact:** ~5-8% speedup in qsearch. Qsearch represents 60-70% of all nodes visited.

### 3b. SEE Caching via `ScoredMove`

**Problem:** `getMoveScore()` calls `pos.see(move)` for capture scoring. Then `quiescence()` calls `pos.see(move)` again on the same captures for SEE pruning. SEE involves iterating through attackers/defenders — expensive.

**Fix:** Introduce `ScoredMove`:
```cpp
// In Types.h or a new SearchTypes.h
struct ScoredMove {
  Move move;
  int score;      // move ordering score
  int seeValue;   // cached SEE, only computed for captures (INT_MIN = not computed)
};
```

**Changes:**
- `orderMoves()` populates `ScoredMove::seeValue` for captures during scoring
- `negamax()` and `quiescence()` use `std::vector<ScoredMove>` instead of `std::vector<Move>`
- Quiescence SEE pruning reads `sm.seeValue` instead of recomputing `pos.see(move)`

**Scope of change:**

- `getMoveScore()` → computes and caches SEE for captures
- `orderMoves()` → builds and sorts a `std::vector<ScoredMove>`, returns it (signature changes from void to returning the vector)
- `negamax()` move loop → iterates `ScoredMove`, reads `sm.move`
- `quiescence()` → builds `ScoredMove` vector during its inline sort (currently at line 665). The sort lambda calls `getMoveScore()` which computes SEE; store the result in `ScoredMove::seeValue`. The SEE pruning loop (line 671) then reads `sm.seeValue` instead of calling `pos.see(move)` again.

**Quiescence detail:** Quiescence does NOT call `orderMoves()` — it sorts inline. The change here is to convert the inline sort to build `ScoredMove` entries with cached SEE, then iterate those. The pattern:

```cpp
std::vector<ScoredMove> scoredCaptures;
scoredCaptures.reserve(captures.size());
for (Move m : captures) {
  int see = pos.see(m);
  int score = getMoveScoreFromSEE(pos, m, see);  // uses pre-computed SEE
  scoredCaptures.push_back({m, score, see});
}
std::sort(scoredCaptures.begin(), scoredCaptures.end(), ...);
// Loop reads sm.seeValue instead of recomputing
```

**Expected impact:** ~3-5% overall speedup. SEE is called O(moves * depth) times during a search; caching halves the calls for captures.

---

## Part 4: Minor Cleanups

### 4a. Unify Header Guards

Change `Polyglot.h` and `Tablebase.h` from `#ifndef`/`#define` guards to `#pragma once`, matching the other 10 headers.

### 4b. Remove Deprecated `seeCapture()`

Remove the deprecated `Position::seeCapture()` method (~5 lines in Position.cpp, declaration in Position.h). It is unused.

---

## Implementation Order

Each step is independently verifiable:

1. **Extract `Eval.h/cpp`** — pure file move, behavior-identical. Verify: `ctest` + perft.
2. **Decompose `negamax()`** — extract 5 helpers, behavior-identical. Verify: `ctest` + perft.
3. **Add `generateCheckingMoves()`** — new MoveGen function + qsearch integration. Verify: `ctest` + perft.
4. **SEE caching** — `ScoredMove` struct + plumb through search. Verify: `ctest` + perft.
5. **Minor cleanups** — header guards + dead code. Verify: `ctest`.

Steps 1-2 are refactors (zero behavior change). Steps 3-4 are performance optimizations (same moves generated, faster). Step 5 is trivial.

## Risk Assessment

| Step | Risk | Mitigation |
|------|------|------------|
| Eval extraction | Low | Pure move, no logic change |
| negamax decomposition | Medium | Must preserve exact alpha/beta/PV behavior. Perft + self-play game comparison |
| generateCheckingMoves | Low | Additive (new function). Existing captures unchanged |
| SEE caching | Low | Same values, just cached. SEE correctness already tested |
| Header guards | Trivial | Compile check only |

## Files Modified

| File | Change |
|------|--------|
| `inc/Eval.h` | **New** — namespace Eval, single `evaluate()` function |
| `src/Eval.cpp` | **New** — PST + evaluate + 8 helper functions |
| `inc/AI.h` | Remove eval methods, add search helper declarations, add `#include <optional>` |
| `src/AI.cpp` | Remove eval code, extract 5 helpers from negamax, use ScoredMove, update qsearch |
| `inc/MoveGen.h` | Add `generateCheckingMoves()` declaration |
| `src/MoveGen.cpp` | Add `generateCheckingMoves()` implementation |
| `inc/Types.h` | Add `ScoredMove` struct |
| `inc/Polyglot.h` | Change to `#pragma once` |
| `inc/Tablebase.h` | Change to `#pragma once` |
| `src/Position.cpp` | Remove deprecated `seeCapture()` |
| `inc/Position.h` | Remove `seeCapture()` declaration |
| `CMakeLists.txt` | Add `src/Eval.cpp` to source list |
