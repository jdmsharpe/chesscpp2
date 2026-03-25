# King Safety Eval Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve king safety evaluation so the engine stops losing games where it's up material but its king is fatally exposed, and stops overvaluing sacrifices for attacks that can be defused by trading queens.

**Architecture:** Two targeted changes to the static `evaluateKingSafety()` function in `Eval.cpp`. Change 1 scales the attack penalty by queen presence. Change 2 amplifies the penalty when the defending side has more material. Both compose multiplicatively within the existing quadratic penalty framework. A test-only wrapper exposes `evaluateKingSafety()` for direct unit testing.

**Tech Stack:** C++20, Google Test

**Spec:** `docs/superpowers/specs/2026-03-24-king-safety-eval-improvements-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/Eval.cpp` | Modify | Add queen-scaling and material-scaling to `evaluateKingSafety()`, add test wrapper |
| `inc/Eval.h` | Modify | Declare `kingSafetyForTest()` in `Eval` namespace |
| `test/test_eval.cpp` | Modify | Add 4 new king safety tests at end of file (after line 412) |
| `.claude/CLAUDE.md` | Modify | Update eval subsystem description |

---

## Task 1: Expose King Safety for Direct Testing

**Files:**
- Modify: `inc/Eval.h`
- Modify: `src/Eval.cpp`

The current `evaluateKingSafety()` is `static` (file-local), so tests can only reach it through the full `Eval::evaluate()` — where material differences drown out king safety effects. Adding a thin public wrapper lets tests check king safety scores directly.

- [ ] **Step 1: Add declaration to Eval.h**

In `inc/Eval.h`, add inside the `Eval` namespace:

```cpp
namespace Eval {
int evaluate(const Position& pos);

// Exposed for testing — returns raw king safety score for the given side.
// Positive = safer king. Negative = king under attack.
int kingSafetyForTest(const Position& pos, Color c);
}  // namespace Eval
```

- [ ] **Step 2: Add implementation to Eval.cpp**

At the end of the `Eval` namespace block in `src/Eval.cpp` (after the `evaluate()` function, before the closing `}`), add:

```cpp
int kingSafetyForTest(const Position& pos, Color c) {
  return evaluateKingSafety(pos, c);
}
```

- [ ] **Step 3: Build to verify it compiles**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . --target test_eval 2>&1 | tail -5`

Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add inc/Eval.h src/Eval.cpp
git commit -m "refactor: expose evaluateKingSafety for direct unit testing"
```

---

## Task 2: Queen-Scaling Tests

**Files:**
- Modify: `test/test_eval.cpp:412` (append after last test)

- [ ] **Step 1: Write queen-scaling tests**

Add to the end of `test/test_eval.cpp`:

```cpp
// =============================================================================
// King Safety — Queen-Presence Scaling
// =============================================================================

TEST_F(EvalTest, KingSafety_QueenScaling_AttackWeakerWithoutQueen) {
  // White king on g1 with pawns f2,g2,h2. Black Q on e4 + N on f3 attack
  // the king zone (Qe4 hits g2, Nf3 hits g1+h2). 2 attackers = penalty fires.
  Position posWithQueen;
  posWithQueen.setFromFEN("6k1/8/8/8/4q3/5n2/5PPP/6K1 w - - 0 1");
  int safetyWithQueen = Eval::kingSafetyForTest(posWithQueen, WHITE);

  // Same setup but Black's queen replaced with a bishop on e4.
  // Be4 attacks g2 (in king zone) via diagonal, Nf3 attacks g1+h2.
  // Still 2 attackers, but attacker has no queen → penalty should be scaled down.
  Position posNoQueen;
  posNoQueen.setFromFEN("6k1/8/8/8/4b3/5n2/5PPP/6K1 w - - 0 1");
  int safetyNoQueen = Eval::kingSafetyForTest(posNoQueen, WHITE);

  // Without attacker's queen, the king safety penalty should be smaller
  // (less negative = higher score = safer king).
  // The queen also contributes more attack units (5 vs 2), but the queen-scaling
  // multiplier (100% vs 25%) is the dominant factor we're testing.
  EXPECT_GT(safetyNoQueen, safetyWithQueen)
      << "King safety without attacker queen (" << safetyNoQueen
      << ") should be better than with queen (" << safetyWithQueen << ")";
}

TEST_F(EvalTest, KingSafety_QueenScaling_PenaltyDropsSignificantly) {
  // Quantitative check: the penalty with queen should be at least 3x
  // the penalty without queen (accounting for both queen-scaling and
  // different attack units: Q=5 vs B=2 units).
  //
  // With queen (2 attackers, 7 units): base penalty = 7*7/2 = 24, scaled 100% = 24
  // Without queen (2 attackers, 4 units): base penalty = 4*4/2 = 8, scaled 25% = 2
  // Ratio: 24/2 = 12x. We conservatively test >= 3x.
  Position posWithQueen;
  posWithQueen.setFromFEN("6k1/8/8/8/4q3/5n2/5PPP/6K1 w - - 0 1");
  int safetyWith = Eval::kingSafetyForTest(posWithQueen, WHITE);

  Position posNoQueen;
  posNoQueen.setFromFEN("6k1/8/8/8/4b3/5n2/5PPP/6K1 w - - 0 1");
  int safetyNo = Eval::kingSafetyForTest(posNoQueen, WHITE);

  // Both positions have same pawn shield, so the pawn shield component cancels.
  // Compute the attack penalty portion (the negative part beyond shield).
  // A position with no attackers and the same pawns gives us the shield baseline.
  Position posNoAttack;
  posNoAttack.setFromFEN("6k1/8/8/8/8/8/5PPP/6K1 w - - 0 1");
  int safetyBaseline = Eval::kingSafetyForTest(posNoAttack, WHITE);

  int penaltyWith = safetyBaseline - safetyWith;    // positive = larger penalty
  int penaltyNo = safetyBaseline - safetyNo;

  EXPECT_GE(penaltyWith, penaltyNo * 3)
      << "Queen-present penalty (" << penaltyWith
      << ") should be at least 3x the no-queen penalty (" << penaltyNo << ")";
}
```

- [ ] **Step 2: Build and run to check baseline**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . --target test_eval 2>&1 | tail -5 && ./test/test_eval --gtest_filter="*QueenScaling*"`

Expected: `AttackWeakerWithoutQueen` likely PASSES already (queen gives more attack units, so higher base penalty). `PenaltyDropsSignificantly` likely FAILS (current ratio is 24:8 = 3x, not the 12x we'd get after queen-scaling).

- [ ] **Step 3: Commit test scaffolding**

```bash
git add test/test_eval.cpp
git commit -m "test: add queen-scaling king safety tests"
```

---

## Task 3: Material-Scaling Tests

**Files:**
- Modify: `test/test_eval.cpp` (append after Task 2's tests)

- [ ] **Step 1: Write material-scaling tests**

Add to the end of `test/test_eval.cpp`:

```cpp
// =============================================================================
// King Safety — Material-Scaled King Danger
// =============================================================================

TEST_F(EvalTest, KingSafety_MaterialScaling_HigherPenaltyWhenUpMaterial) {
  // White king on g2, Black Q on e4 + N on f3 attack the king zone.
  // White has a queen + 2 rooks elsewhere = lots of material.
  // Being up material should AMPLIFY the king danger penalty.
  Position posUpMaterial;
  posUpMaterial.setFromFEN("6k1/8/8/3Q4/4q3/5n2/5PKP/RR6 w - - 0 1");
  int safetyUp = Eval::kingSafetyForTest(posUpMaterial, WHITE);

  // Same attack, but White has only pawns (no extra material).
  // King danger should be at base rate (no amplification).
  Position posEven;
  posEven.setFromFEN("6k1/8/8/8/4q3/5n2/5PKP/8 w - - 0 1");
  int safetyEven = Eval::kingSafetyForTest(posEven, WHITE);

  // White's king safety should be WORSE (more negative) when up material,
  // because the material-scaling amplifies the penalty.
  // Both positions have the same pawn shield and same attackers.
  EXPECT_LT(safetyUp, safetyEven)
      << "King safety when up material (" << safetyUp
      << ") should be worse than at even material (" << safetyEven
      << ") due to amplified danger";
}

TEST_F(EvalTest, KingSafety_MaterialScaling_NoAmplificationWhenDown) {
  // Same attack configuration, but White is DOWN material.
  // Penalty should NOT be amplified (only when up material).
  Position posDown;
  posDown.setFromFEN("6k1/8/8/8/4q3/5n2/5PKP/8 w - - 0 1");
  int safetyDown = Eval::kingSafetyForTest(posDown, WHITE);

  // White has even less material (no pawns besides king shelter).
  Position posMoreDown;
  posMoreDown.setFromFEN("6k1/8/8/8/4q3/5n2/5PKP/8 w - - 0 1");
  int safetyMoreDown = Eval::kingSafetyForTest(posMoreDown, WHITE);

  // When down material, both should have the same king safety score
  // (no amplification either way).
  EXPECT_EQ(safetyDown, safetyMoreDown);
}
```

- [ ] **Step 2: Build and run to check baseline**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . --target test_eval 2>&1 | tail -5 && ./test/test_eval --gtest_filter="*MaterialScaling*"`

Expected: `HigherPenaltyWhenUpMaterial` FAILS (current eval gives same penalty regardless of material). `NoAmplificationWhenDown` PASSES (both positions are identical — same penalty).

- [ ] **Step 3: Commit test scaffolding**

```bash
git add test/test_eval.cpp
git commit -m "test: add material-scaling king safety tests"
```

---

## Task 4: Implement Queen-Presence Scaling

**Files:**
- Modify: `src/Eval.cpp` — the quadratic penalty block inside `evaluateKingSafety()`

- [ ] **Step 1: Replace the quadratic penalty block**

In `src/Eval.cpp`, find and replace this block inside `evaluateKingSafety()`:

```cpp
  // Non-linear penalty: attacks become exponentially more dangerous
  // Only apply when at least 2 pieces are attacking (1 attacker is normal)
  if (numAttackers >= 2) {
    score -= attackUnits * attackUnits / 2;
  }
```

With:

```cpp
  // Non-linear penalty: attacks become exponentially more dangerous
  // Only apply when at least 2 pieces are attacking (1 attacker is normal)
  if (numAttackers >= 2) {
    int penalty = attackUnits * attackUnits / 2;

    // Queen-presence scaling: attacks without a queen are far less likely
    // to lead to mate, so reduce the penalty significantly
    bool attackerHasQueen = (pos.pieces(them, QUEEN) != 0);
    if (!attackerHasQueen) {
      bool defenderHasQueen = (pos.pieces(c, QUEEN) != 0);
      penalty = defenderHasQueen ? penalty * 40 / 100   // Only defender has queen
                                 : penalty * 25 / 100;  // Neither side has queen
    }

    score -= penalty;
  }
```

- [ ] **Step 2: Build and run queen-scaling tests**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . --target test_eval 2>&1 | tail -5 && ./test/test_eval --gtest_filter="*QueenScaling*"`

Expected: Both queen-scaling tests PASS.

- [ ] **Step 3: Run full eval test suite to check for regressions**

Run: `cd /home/onyx/chesscpp2/build && ./test/test_eval`

Expected: All eval tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/Eval.cpp
git commit -m "feat: add queen-presence scaling to king safety eval

Scale king attack penalty by attacker's queen presence:
- Attacker has queen: 100% (unchanged)
- No attacker queen, defender has queen: 40%
- Neither side has queen: 25%"
```

---

## Task 5: Implement Material-Scaled King Danger

**Files:**
- Modify: `src/Eval.cpp` — the `if (numAttackers >= 2)` block inside `evaluateKingSafety()` (modified in Task 4)

- [ ] **Step 1: Add material scaling after queen scaling**

In `src/Eval.cpp`, within the `if (numAttackers >= 2)` block, add material scaling after the queen-presence scaling and before `score -= penalty`:

```cpp
    // Material-scaled king danger: amplify penalty when defending side has
    // more material (more to lose = more reason to worry about king safety)
    int materialAdv = pos.materialCount(c) - pos.materialCount(them);
    if (materialAdv > 0) {
      int capped = std::min(materialAdv, 800);
      int dangerScale = 256 + capped * 128 / 800;
      penalty = penalty * dangerScale / 256;
    }
```

The full block should now read:

```cpp
  if (numAttackers >= 2) {
    int penalty = attackUnits * attackUnits / 2;

    // Queen-presence scaling: attacks without a queen are far less likely
    // to lead to mate, so reduce the penalty significantly
    bool attackerHasQueen = (pos.pieces(them, QUEEN) != 0);
    if (!attackerHasQueen) {
      bool defenderHasQueen = (pos.pieces(c, QUEEN) != 0);
      penalty = defenderHasQueen ? penalty * 40 / 100   // Only defender has queen
                                 : penalty * 25 / 100;  // Neither side has queen
    }

    // Material-scaled king danger: amplify penalty when defending side has
    // more material (more to lose = more reason to worry about king safety)
    int materialAdv = pos.materialCount(c) - pos.materialCount(them);
    if (materialAdv > 0) {
      int capped = std::min(materialAdv, 800);
      int dangerScale = 256 + capped * 128 / 800;
      penalty = penalty * dangerScale / 256;
    }

    score -= penalty;
  }
```

- [ ] **Step 2: Build and run material-scaling tests**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . --target test_eval 2>&1 | tail -5 && ./test/test_eval --gtest_filter="*MaterialScaling*"`

Expected: Both material-scaling tests PASS.

- [ ] **Step 3: Run full eval test suite**

Run: `cd /home/onyx/chesscpp2/build && ./test/test_eval`

Expected: All eval tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/Eval.cpp
git commit -m "feat: add material-scaled king danger to eval

Amplify king safety penalty when defending side has more material:
- Even/behind: 100% (unchanged)
- +400cp: 125%
- +800cp+: 150% (capped)

Composes with queen-presence scaling from previous commit."
```

---

## Task 6: Full Test Suite + Perft Verification

**Files:** None modified — verification only.

- [ ] **Step 1: Run all tests (170 after adding 4 new tests)**

Run: `cd /home/onyx/chesscpp2/build && cmake --build . 2>&1 | tail -5 && ctest --output-on-failure 2>&1 | tail -5`

Expected: `100% tests passed, 0 tests failed out of 170`

- [ ] **Step 2: Run perft verification**

Run: `cd /home/onyx/chesscpp2 && bash scripts/verify_perft.sh`

Expected: All perft counts match (depth 1=20, 2=400, 3=8902, 4=197281, 5=4865609).

- [ ] **Step 3: Commit if any test fixes were needed (skip if clean)**

---

## Task 7: Replay Lost Positions

**Files:** None modified — verification only. User participation: both do this together.

- [ ] **Step 1: Test loss 2 position (promotion tunnel vision)**

The engine should now evaluate this position more cautiously for White (own king danger amplified because White is up material):

```
Position from game 3 (174004 run), before the e-pawn push:
FEN: rnb2k2/3Pp1p1/3N4/B7/8/8/1b1P1nPP/R4RK1 w - - 0 27
```

Run the engine at depth 12 on this position and check if it still pushes e7/e8=Q or finds a safer continuation. This is a manual verification step.

- [ ] **Step 2: Test loss 1 position (speculative sacrifice)**

Check if the engine still evaluates ...Bxh3 as good in the pre-sacrifice position:

```
Position from game 2 (174004 run), before ...Bxh3:
FEN: r4rk1/pp1q1pp1/2n2n1p/4p3/2P1P3/4B2P/PP1NBPP1/R2Q1RK1 b - - 0 13
```

Run at depth 12. The engine should now evaluate ...Bxh3 lower because queen-scaling would reduce the attack penalty after the forced queen trade.

- [ ] **Step 3: Document results**

Note whether the engine's evaluation changed for both positions. These are sanity checks, not hard pass/fail — the eval changes may or may not change the engine's top move choice depending on depth and other factors.

---

## Task 8: Update Documentation

**Files:**
- Modify: `.claude/CLAUDE.md`

- [ ] **Step 1: Update the Eval subsystem description**

In the Architecture table under the Evaluation row, update the description to mention the new features. Find the text:

```
king safety with attack unit counting (quadratic penalty)
```

Replace with:

```
king safety with attack unit counting (quadratic penalty, queen-presence scaling, material-scaled danger)
```

- [ ] **Step 2: Commit**

```bash
git add .claude/CLAUDE.md
git commit -m "docs: update CLAUDE.md with king safety eval improvements"
```
