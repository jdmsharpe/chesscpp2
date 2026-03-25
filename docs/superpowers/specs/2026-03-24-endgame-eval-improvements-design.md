# Endgame Evaluation Improvements

**Date**: 2026-03-24
**Status**: Draft
**Scope**: Eval-only changes to endgame scoring in `Eval.cpp`

## Problem

The engine loses long endgames (100-200 plies) against Stockfish level 8, particularly:
- Games where it has passed pawns but can't convert them
- K+P endings where its king shuffles instead of centralizing

Combined level-8 record: 5W, 3D, 6L across 3 tournament runs (~46% score). Most losses are as Black in games exceeding 100 plies, where the engine gets ground down.

### Root causes

1. **Passed pawn advancement bonus may be too conservative**: `evaluatePawnStructureSide()` (lines 81-97) already has an advancement bonus table `{10, 15, 25, 40, 70, 120}` plus a flat +20 base and +15 clear-path bonus. However, the engine still trades away passers too easily, suggesting these values don't sufficiently incentivize keeping and pushing passed pawns. The pawn structure score gets 1.5x weight in endgames, making the effective 7th-rank bonus ~232cp — which may still be insufficient against a strong opponent's counterplay.

2. **King centralization in endgames relies solely on static PST**: The EG king PST gives +20 for center squares (d4/d5/e4/e5). In roughly equal endgames where mop-up doesn't activate (requires phase ≤128 AND ≥400cp material advantage), the king has insufficient incentive to centralize beyond this small PST value.

## Design

### Change 1: Tune Passed Pawn Advancement Values

Increase the existing advancement bonus values in `evaluatePawnStructureSide()` to make the engine value advanced passers more highly.

**Location**: `evaluatePawnStructureSide()` in `Eval.cpp`, line 83.

**Current values**:
```cpp
static constexpr int advancementBonus[6] = {10, 15, 25, 40, 70, 120};
```

**New values**:
```cpp
static constexpr int advancementBonus[6] = {10, 20, 35, 55, 90, 150};
```

**Comparison** (index = ranks advanced from starting position):

| Index | Rank (White) | Current | New | Delta | Rationale |
|-------|-------------|---------|-----|-------|-----------|
| 0 | 3rd (1 step) | 10 | 10 | 0 | Minor advance, no change needed |
| 1 | 4th (2 steps) | 15 | 20 | +5 | Slightly more credit |
| 2 | 5th (3 steps) | 25 | 35 | +10 | Past the midpoint, starting to be a threat |
| 3 | 6th (4 steps) | 40 | 55 | +15 | Requires active blockade |
| 4 | 7th (5 steps) | 70 | 90 | +20 | Very dangerous, demands a piece |
| 5 | 8th (6 steps) | 120 | 150 | +30 | Nearly unstoppable |

The increases are progressive — bigger bumps for more advanced passers. The total effective bonus for a 7th-rank passer with clear path becomes: `20 (base) + 150 (advancement) + 15 (clear path) = 185cp` raw, or `~278cp` with the 1.5x endgame weight. This makes a 7th-rank passer roughly worth a minor piece in endgame evaluation, which matches practical chess intuition.

**Note**: These values affect the pawn structure score, which is included in BOTH opening (1.0x) and endgame (1.5x) formulas. The increases are moderate enough not to distort middlegame evaluation — a 5th-rank passer getting +10 more in the middlegame is minor.

### Change 2: Dynamic King Centralization Bonus

Add an endgame bonus for king centralization that supplements the static EG PST.

**Location**: New static function `evaluateKingCentralization()` in `Eval.cpp`, called from `evaluate()`.

**Current gap**: King endgame positioning comes from:
- `kingPositionalEG` (PST): fixed +20 for center, -20 for corners
- `evaluateMopUp()`: only fires at phase ≤128 with ≥400cp material advantage

In roughly equal endgames, neither provides sufficient king activity incentive.

**New function**:

```cpp
static int evaluateKingCentralization(const Position& pos, Color c) {
  Square kingSq = BB::lsb(pos.pieces(c, KING));
  return (6 - centerDistance(kingSq)) * 8;
}
```

`centerDistance()` (Eval.cpp line 488) returns Manhattan distance from center: 0 for d4/e4/d5/e5, up to 6 for corners (a1/a8/h1/h8).

**Bonus values**:

| King position | Center distance | Bonus |
|---------------|----------------|-------|
| d4/d5/e4/e5 | 0 | 48 |
| c3-c6/d3/f3-f6/etc | 1 | 40 |
| b2-b7/g2-g7/etc | 2 | 32 |
| Edge squares | 3-4 | 16-24 |
| a1/a8/h1/h8 | 6 | 0 |

**Integration into `evaluate()`**:

```cpp
int kingCentralization = evaluateKingCentralization(pos, WHITE)
                       - evaluateKingCentralization(pos, BLACK);
```

Added to `endgameScore` only — the tapered eval phases it in naturally. Not included in `openingScore` because king centralization is harmful in middlegames.

**Known limitation**: In some endgames with wing pawns, the king should stay near its own pawns rather than centralize. The king-pawn proximity bonus in `evaluateKingPawnProximity()` counteracts this — a king near its passed pawns gets both proximity credit and potentially centralization credit if the pawns are central. For wing pawn endings, the proximity bonus should outweigh the centralization bonus (proximity can reach ~35cp per passer, while centralization tops at 48cp total). This is an acceptable tradeoff for a first implementation.

### Change 3: Tune Endgame Weight for King-Pawn Proximity

Increase the weight of the `kingPawnProx` term in the endgame score formula.

**Location**: `evaluate()` in `Eval.cpp`, the endgame score formula (lines 598-600).

**Current**:
```cpp
int endgameScore = material + (positional / 2) + kingPositionalEG + (mobility / 2) +
                   (kingSafety / 4) + (pawnStructure * 3 / 2) + (rookScore * 3 / 2) +
                   bishopScore + knightScore + kingPawnProx;
```

**New** (two changes — `kingPawnProx` weight and new `kingCentralization` term):
```cpp
int endgameScore = material + (positional / 2) + kingPositionalEG + (mobility / 2) +
                   (kingSafety / 4) + (pawnStructure * 3 / 2) + (rookScore * 3 / 2) +
                   bishopScore + knightScore + (kingPawnProx * 3 / 2) + kingCentralization;
```

- `kingPawnProx` scaled from 1.0x to 1.5x — king proximity to passed pawns (and the unstoppable-passer detection) should be weighted at least as heavily as pawn structure in endgames
- `kingCentralization` added (new term from Change 2)

## Testing Strategy

### Test wrappers
Expose `evaluateKingPawnProximity()` and `evaluateKingCentralization()` for direct testing via public wrappers in the `Eval` namespace, following the `kingSafetyForTest()` pattern.

### Unit tests
1. **King centralization**: Verify that a centralized king (d4) scores higher than a corner king (a1) via direct wrapper
2. **Passer advancement**: Verify that the updated values are applied — a 6th-rank passer should score significantly higher than a 3rd-rank passer via full `Eval::evaluate()` on carefully chosen endgame positions
3. **Regression**: All existing 170 tests pass, perft clean

### Tournament validation (user runs)
Re-run `depth12_vs_stockfish8.py` multiple times. Baseline: ~33% win rate (5W, 3D, 6L = 6.5/14 across 3 runs). Looking for:
- Improved win rate, especially as Black
- Fewer long grinding losses (100+ ply games that the engine loses)
- No increase in short tactical losses

## Files Changed

| File | Change |
|------|--------|
| `src/Eval.cpp` | Tune advancement bonus values in `evaluatePawnStructureSide()`, add `evaluateKingCentralization()`, tune `kingPawnProx` weight to 1.5x, add `kingCentralization` to endgame score, add test wrappers |
| `inc/Eval.h` | Declare `kingPawnProximityForTest()` and `kingCentralizationForTest()` |
| `test/test_eval.cpp` | Add king centralization and passer advancement unit tests |
| `.claude/CLAUDE.md` | Update eval subsystem description |
