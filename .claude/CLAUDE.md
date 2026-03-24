# CLAUDE.md

Chess++ — bitboard chess engine with magic bitboards, UCI support, Syzygy tablebases, and Polyglot opening books. C++20.

## Build & Run

```bash
mkdir -p build && cd build && cmake .. && cmake --build .

# From build/ directory:
./chesscpp2              # GUI mode (default)
./chesscpp2 -c -d 6      # Play vs AI at depth 6 (default)
./chesscpp2 --nogui -c   # Console mode
./chesscpp2 --perft 5    # Perft test to depth 5
./chesscpp2 --uci        # UCI mode (for GUIs/tournaments)
```

## Testing

```bash
# From build/ directory
ctest                          # Run all tests (154 tests)
./test/test_bitboard           # Bitboard operations
./test/test_movegen            # Move generation + generateCheckingMoves
./test/test_position           # Position/make-unmake/SEE/draw detection
./test/test_polyglot           # Polyglot book tests
./test/test_eval               # Evaluation function (material, PST, pawn structure, king safety)
./test/test_ai                 # Search (mate detection, TT, time management, move ordering)
./test/test_uci                # UCI protocol (command parsing, options, handshake)
./test/test_game               # Game logic (moves, draws, checkmate, stalemate, special moves)

# From repo root
bash scripts/verify_perft.sh   # Verify perft node counts against known values (CI uses this)
```

**Perft correctness** — expected from starting position: depth 1 = 20, depth 2 = 400, depth 3 = 8,902, depth 4 = 197,281, depth 5 = 4,865,609. Any deviation = move generation bug.

## Dependencies

- CMake 3.16+, C++20 (GCC 10+ / Clang 10+)
- SDL2, SDL2_image: `sudo apt-get install libsdl2-dev libsdl2-image-dev`
- Google Test (auto-fetched by CMake)

## Architecture

Board uses 12 bitboards (6 piece types × 2 colors) + piece array for O(1) `pieceAt()`. Sliding piece attacks use magic bitboards for O(1) lookup (see `Magic.h/cpp`).

Moves are 16-bit integers: bits 0-5 from, 6-11 to, 12-13 promotion piece, 14-15 flags (normal/promotion/EP/castling). See `Types.h`.

### Key Subsystems

| Subsystem | Files | Notes |
|-----------|-------|-------|
| Board/position | `Position.h/cpp` | make/unmakeMove, FEN parsing |
| Move generation | `MoveGen.h/cpp` | Pseudo-legal → legality filter |
| Magic bitboards | `Magic.h/cpp` | Pre-computed sliding attack tables |
| Bitboard ops | `Bitboard.h/cpp` | Attack lookups, bit manipulation |
| Search | `AI.h/cpp` | Alpha-beta with helpers: probeTT, storeTT, tryNullMovePruning, canPrune, searchMove; repetition detection in search + qsearch |
| Evaluation | `Eval.h/cpp` | PST (incl. endgame king PST), tapered eval, pawn structure (passed pawn advancement, clear path), king safety, mobility, development, rook-behind-passer, king-passer proximity, mop-up |
| UCI protocol | `UCI.h/cpp` | Standard UCI + time controls |
| Opening books | `Polyglot.h/cpp` | Polyglot .bin format, fallback to `book.txt` |
| Tablebases | `Tablebase.h/cpp` | Syzygy via Fathom (`lib/Fathom`) |
| Zobrist hashing | `Zobrist.h/cpp` | Position hashing for TT and repetition |
| GUI | `Window.h/cpp` | SDL2 rendering |
| Game logic | `Game.h/cpp` | Game controller, draw detection, rules |
| Entry point | `main.cpp` | CLI arg parsing |

### Scripts (`scripts/`)

- `self_play_test.py` — engine self-play
- `tournament.py` — multi-engine tournaments
- `watch_game.py` — visualize games in progress
- `diagnose.py` — engine diagnostics
- `depth8_vs_stockfish.py` — Stockfish benchmark
- `test_preference.py` — preference testing
- `verify_perft.sh` — perft verification (used by CI)

## Critical Gotchas

### unmakeMove must fully reverse makeMove

`unmakeMove()` in `Position.cpp` must undo ALL of `makeMove()`: normal captures, promotions (remove promoted piece, restore pawn), en passant (restore captured pawn on correct square), castling (move both king and rook back). Incomplete unmake → position corruption and segfaults.

### Edge wrapping in diagonal move generation

Sliding piece diagonal generation must check for board edge wrapping, or bishops/queens will illegally wrap (e.g., f8→h1):

```cpp
if (dir == 9 && fileOf(to) == FILE_A) break;   // NE wrapped
if (dir == 7 && fileOf(to) == FILE_H) break;   // NW wrapped
if (dir == -7 && fileOf(to) == FILE_A) break;  // SW wrapped
if (dir == -9 && fileOf(to) == FILE_H) break;  // SE wrapped
```

### Polyglot uses separate Zobrist keys

Polyglot hashing uses standardized Zobrist keys *different from the engine's internal keys*. Piece ordering: bp=0, wp=1, bn=2, wn=3, bb=4, wb=5, br=6, wr=7, bq=8, wq=9, bk=10, wk=11. En passant only included if capture is possible. Castling encoded as king-captures-rook.

## UCI Options

- `setoption name SyzygyPath value /path/to/syzygy` — Syzygy tablebase directory
- `setoption name BookPath value /path/to/books/Titans.bin` — Polyglot opening book

## Design Documents

- `docs/plans/2025-10-30-lazy-smp-multithreading-design.md` — Lazy SMP parallel search (not yet implemented)
- `docs/superpowers/specs/2026-03-23-ai-refactor-and-perf-design.md` — AI.cpp refactor + performance wins (completed)
- `docs/superpowers/plans/2026-03-23-ai-refactor-and-perf.md` — Implementation plan for the above

## Build System Notes

- Build type defaults to `RelWithDebInfo` but can be overridden: `cmake -DCMAKE_BUILD_TYPE=Release ..`
- `-O3 -march=native` applied only to the main executable, not test targets
- Test CMakeLists uses shared static libraries (`chess_core`, `chess_engine`) to avoid recompiling sources per test target
- CI runs Release, Debug, and sanitizer (ASan+UBSan) builds

## Known Issues

1. Fullmove counter can desync if unmakeMove logic is incorrect
