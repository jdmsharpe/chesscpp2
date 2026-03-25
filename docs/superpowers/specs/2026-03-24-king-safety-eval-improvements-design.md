# King Safety Evaluation Improvements

**Date**: 2026-03-24
**Status**: Draft
**Scope**: Eval-only changes to `evaluateKingSafety()` in `Eval.cpp`

## Problem

Two related king safety evaluation weaknesses observed in tournament play (Chess++ depth-12 vs Stockfish skill-6):

### 1. King safety myopia when winning

The engine pushes passed pawns and accumulates material while ignoring that its own king is fatally exposed. In a tournament game, Chess++ promoted a queen with check (`29. e8=Q+ Kf6`) but immediately lost — its king on g1 was surrounded by Black's Bg2 + Nf4 with an unstoppable mating net.

**Root cause**: The eval treats king danger identically regardless of material balance. A king with 8 attack units against it scores the same penalty whether the side is up 500cp or even. The engine sees "+5 material, -2 king safety = +3 total" and pushes for promotion, missing that the king safety deficit is actually fatal.

### 2. Speculative sacrifice overvaluation

The engine plays material sacrifices for king attacks that aren't forcing. In a tournament game, Chess++ played `...Bxh3` (bishop sacrifice for two pawns and a king attack). The attack looked strong in the eval (queen on h3, open g-file), but Stockfish defused it with `Ng5` forcing a queen trade 5 moves later. After queens came off, Chess++ was just down a bishop.

**Root cause**: The king safety attack penalty fires at full strength even when the attacking side has no queen. Without a queen, king zone pressure rarely leads to mate, but the eval doesn't distinguish this.

## Design

### Change 1: Queen-Presence Scaling

Scale the king safety quadratic penalty based on whether the *attacking* side has a queen.

**Location**: `evaluateKingSafety()` in `Eval.cpp`, after the quadratic penalty computation (currently line ~235).

**Current code**:
```cpp
if (numAttackers >= 2) {
  score -= attackUnits * attackUnits / 2;
}
```

**New behavior**: After computing the base penalty, scale by queen presence:

```cpp
// In evaluateKingSafety(pos, c): c = defending king's color, them = ~c
if (numAttackers >= 2) {
  int penalty = attackUnits * attackUnits / 2;

  // Scale by attacking side's queen presence
  bool attackerHasQueen = (pos.pieces(them, QUEEN) != 0);
  bool defenderHasQueen = (pos.pieces(c, QUEEN) != 0);

  if (!attackerHasQueen) {
    if (!defenderHasQueen) {
      penalty = penalty * 25 / 100;   // Neither side has queen
    } else {
      penalty = penalty * 40 / 100;   // Only defender has queen
    }
  }

  score -= penalty;
}
```

**Scale factors**:
| Attacker queen | Defender queen | Penalty multiplier |
|----------------|----------------|--------------------|
| Yes            | Any            | 100% (unchanged)   |
| No             | Yes            | 40%                |
| No             | No             | 25%                |

**Phase-tapering interaction**: King safety is already scaled from 100% (opening) to 25% (endgame) via the tapered eval in `evaluate()`. When queens are traded, the phase value also drops (queens contribute 4 out of 24 phase weight), which shifts the blend toward endgame scoring. The queen-presence scaling stacks multiplicatively with this existing reduction. At phase 128 (midgame) with no attacker queen: effective penalty = 40% * 62.5% = 25% of base, which remains meaningful for rook-lift attacks. The multipliers (40/25) are set higher than an initial proposal of (25/15) precisely to avoid over-suppressing king danger in queenless middlegames.

**Rationale**: Without a queen, king attacks almost never lead to mate. A knight and bishop in the king zone look threatening in the eval but can't deliver checkmate. This directly fixes the `...Bxh3` failure — after the forced queen trade, the attack units around White's king score at 25% instead of 100%, making the sacrifice correctly evaluate as a material loss.

The 100/25/15 values are starting points. The key insight is qualitative: queen-present attacks are fundamentally different from queen-absent attacks.

### Change 2: Material-Scaled King Danger

Increase the king safety penalty for a side that has more material. The more material you have, the more the eval "worries" about your exposed king.

**Location**: Same function, applied after queen-scaling (Change 1).

**New behavior**: After computing the (queen-scaled) penalty, multiply by a material-awareness factor:

```cpp
// material[c] includes all piece values (pawns + pieces, excluding king)
int myMaterial = pos.materialCount(c);
int theirMaterial = pos.materialCount(them);
int materialAdv = myMaterial - theirMaterial;

if (materialAdv > 0) {
  int capped = std::min(materialAdv, 800);
  int dangerScale = 256 + capped * 128 / 800;
  penalty = penalty * dangerScale / 256;
}
```

**Scale factors**:
| Material advantage | Danger multiplier | Example              |
|--------------------|-------------------|----------------------|
| Even or behind     | 100% (256/256)    | Normal evaluation    |
| +200cp             | ~113% (288/256)   | Up a minor exchange  |
| +400cp             | ~125% (320/256)   | Up a rook            |
| +600cp             | ~138% (352/256)   | Up rook + minor      |
| +800cp+            | 150% (384/256)    | Up a queen (capped)  |

**Rationale**: Directly addresses the "promotion tunnel vision" game. When Chess++ was up material but its king was naked, the eval said "moderate king danger." With this change, being up material *amplifies* the king danger signal. The engine becomes paranoid about its own king precisely when it has "more to lose."

The cap at 800cp prevents pathological behavior where being up a queen makes every slightly-open king position look dangerous.

**Why amplify danger rather than dampen material?** Reducing the material score would affect the entire eval — move ordering, aspiration windows, TT entries. Amplifying king danger is surgical: it only changes the eval when there's actual king danger AND a material imbalance, preserving the material signal for all other purposes.

### Interaction Between Changes

The two changes compose naturally. For a position where:
- The defending side is up 400cp (rook)
- The attacking side has no queen
- There are 6 attack units from 2 pieces

**Current eval**: `penalty = 6*6/2 = 18`

**New eval (integer arithmetic)**: `penalty = (18 * 40 / 100) * 320 / 256 = 7 * 320 / 256 = 8`

Without a queen, the attack isn't dangerous, even though the defender is up material. The material scaling amplifies danger, but the queen-absence scaling dominates.

Conversely, if the attacker DOES have a queen:
**New eval**: `penalty = (18 * 100 / 100) * 320 / 256 = 18 * 320 / 256 = 22`

The attack is taken more seriously because the defender has more to lose.

## Material Counting

The `evaluateKingSafety()` function needs access to material values per side. The `Position` class maintains incremental `material[2]` accumulators (indexed by color) via `putPiece()`/`removePiece()`, exposed through `materialCount(Color)`. These accumulators include all piece values (pawns at 100cp, knights at 320cp, etc.) except kings (0cp). Including pawn material is acceptable — more total material means more to lose regardless of whether it's pawns or pieces.

## Testing Strategy

### Automated tests (we implement together)
1. **Existing test suite** — All 166 tests must pass
2. **Queen-scaling unit tests** — Same position with/without queens, verify penalty changes
3. **Material-scaling unit tests** — Same king danger at different material balances, verify amplification
4. **Perft verification** — `verify_perft.sh` (eval-only changes, but confirms no side effects)
5. **Replay lost positions** — FEN from the two tournament losses, verify the engine now avoids the losing moves at depth 12

### Tournament validation (user runs)
6. **Re-run `depth12_vs_stockfish6.py`** multiple times, compare against baseline (~83% win rate)
   - No increase in losses
   - Fewer games where the engine loses while up material
   - No significant increase in game length (would indicate over-cautious play)

## Files Changed

| File | Change |
|------|--------|
| `src/Eval.cpp` | Modify `evaluateKingSafety()` — add queen-scaling and material-scaling |
| `test/test_eval.cpp` | Add queen-scaling and material-scaling unit tests |
| `.claude/CLAUDE.md` | Update eval subsystem description |
