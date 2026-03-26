# Self-Play Color Balance Analysis

**Date:** 2026-03-25
**Engine:** Chess++ (current main branch)
**Method:** Self-play via `scripts/self_play_test.py`, two identical engine instances alternating colors

## Test Configuration

| Parameter | Value |
|-----------|-------|
| Depth | 10 |
| Hash | 64 MB |
| Games | 100 (50 per color per instance) |
| Opening book | Titans.bin (Polyglot) |
| Tablebases | Syzygy (3-4-5 piece) |
| Draw adjudication | python-chess (threefold, 50-move, insufficient material) |

## Results

### Color Balance

| | Wins | Score | Percentage |
|---|---|---|---|
| White | 26 | 51.0/100 | 51.0% |
| Black | 24 | 49.0/100 | 49.0% |

**Binomial p-value: 0.89** -- no statistically significant color imbalance.

A 51-49 White advantage is well within normal variance and actually smaller than the ~53-55% White edge seen in professional human chess. The engine plays balanced chess from both sides.

### Game Outcomes

| Termination | Count | Percentage |
|---|---|---|
| Checkmate | 50 | 50% |
| Threefold repetition | 48 | 48% |
| Insufficient material | 2 | 2% |
| Stalemate | 0 | 0% |
| 50-move rule | 0 | 0% |

### First Half vs Second Half

| Segment | White wins | Black wins | Draws | Draw rate |
|---|---|---|---|---|
| Games 1-50 | 10 | 10 | 30 | 60% |
| Games 51-100 | 16 | 14 | 20 | 40% |

The draw rate dropped from 60% to 40% in the second half, likely reflecting opening book diversity exhaustion or transposition table residue influencing move selection into sharper lines.

## Key Findings

1. **No color bias** -- 51% White score is statistically indistinguishable from 50%.

2. **Repetition draw dominance** -- 96% of draws (48/50) ended by threefold repetition. Zero 50-move draws, zero stalemates. The engine's contempt logic (`AI.cpp:362-368`) returns 0 when material is within +/-200cp, so it has no incentive to avoid repetition in equal positions. A small base contempt (5-10cp) could reduce repetition tendency.

3. **50% decisive rate** -- healthy for a depth-10 self-play test. By comparison, a prior 10-game depth-12 run showed a 60% draw rate, consistent with deeper search producing more draws.

4. **Engine instance variance** -- the "Black pref" instance outscored "White pref" 54-46 despite being identical binaries. This is expected random variance from Polyglot book weight sampling and TT interaction between games, not a real strength difference.

## Comparison: Depth 10 vs Depth 12

| Metric | Depth 10 (100 games) | Depth 12 (10 games) |
|---|---|---|
| White score | 51.0% | 50.0% |
| Draw rate | 50% | 60% |
| Threefold repetition draws | 96% of draws | 100% of draws |
| Avg decisive game length | ~118 plies | ~118 plies |

Deeper search increases draw rate as expected -- the engine evaluates positions more accurately and finds more forced equality.

## Contempt Behavior

Current contempt logic in `AI.cpp`:

```cpp
int contempt = 0;
int material = pos.materialCount(pos.sideToMove()) - pos.materialCount(~pos.sideToMove());
if (material > 200)  contempt = -15;   // ahead: avoid draws
else if (material < -200) contempt = 15; // behind: seek draws
```

The +/-200cp threshold means contempt only activates when significantly ahead/behind. In balanced positions (the majority of self-play), contempt is zero, which explains the high threefold repetition rate. Engines like Stockfish use a small positive contempt (~10-25cp) in self-play or rated play to encourage fighting chess.
