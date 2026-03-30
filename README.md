# Chess++ with Bitboards

![Badge](https://hitscounter.dev/api/hit?url=https%3A%2F%2Fgithub.com%2Fjdmsharpe%2Fchesscpp2%2F&label=chesscpp2&icon=github&color=%23198754&message=&style=flat&tz=UTC)
[![CI](https://github.com/jdmsharpe/chesscpp2/actions/workflows/ci.yml/badge.svg)](https://github.com/jdmsharpe/chesscpp2/actions/workflows/ci.yml)

An improved chess engine implementation using bitboards for fast move generation and position evaluation.

## Features

### Core Engine

- **Bitboard representation** - 64-bit integers for efficient board representation
- **Magic bitboards** - Ultra-fast sliding piece (rook, bishop, queen) move generation
- **Fast move generation** - Optimized legal move generation using bit operations
- **AI with alpha-beta pruning** - Minimax search with Lazy SMP multi-threading, staged move generation (MovePicker), shared lockless transposition table, logarithmic LMR, singular extensions, killer/history/countermove heuristics, adaptive null move pruning, aspiration windows, and quiescence search
- **Lazy SMP multi-threading** - Parallel search with shared transposition table, per-thread search heuristics, depth-skip diversity, best-thread result selection, helper aspiration-window variation, LMR/null-move variation; configurable 1-256 threads
- **Stack-allocated move lists** - Zero-heap-allocation `MoveList` using fixed-capacity arrays for all move generation hot paths (+11% NPS)
- **Incremental evaluation** - PST scores and material maintained incrementally in make/unmake for O(1) eval lookups; tapered eval, pawn hash table, and king safety attack units
- **UCI protocol** - Integration with chess GUIs and tournament software
- **FEN support** - Load and save positions in Forsyth-Edwards Notation
- **Syzygy tablebases** - Perfect endgame play with 3-4-5 piece tablebases
- **Polyglot opening books** - Industry-standard .bin format opening books

### User Interface

- **SDL2 GUI** - Graphical chess board with mouse input
- **Console mode** - Text-based interface for terminal play
- **Game animator** - Pygame-based PGN viewer with sliding pieces, fading move arrows, and playback controls
- **Interactive features**:
  - Click to select and move pieces
  - Visual highlighting of selected squares and legal moves
  - AI opponent with adjustable search depth
  - Position loading from FEN strings or files
  - Perft testing for move generation verification

## Building

### Requirements

- CMake 3.16 or higher
- C++20 compatible compiler (GCC, Clang, or MSVC)
- SDL2 and SDL2_image libraries

### Build Instructions

```bash
mkdir build
cd build
cmake ..              # defaults to RelWithDebInfo
cmake --build .

# Or specify build type:
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Installing SDL2 on Ubuntu/Debian

```bash
sudo apt-get install libsdl2-dev libsdl2-image-dev
```

### Installing SDL2 on macOS

```bash
brew install sdl2 sdl2_image
```

### Python Dependencies (for scripts)

```bash
pip install -r requirements.txt
```

## Usage

### Basic Commands

```bash
# Run with GUI (default)
./chesscpp2

# Play against AI (default depth is 8)
./chesscpp2 -c

# Override AI search depth
./chesscpp2 -c -d 10

# Run in console mode (no GUI)
./chesscpp2 --nogui

# Load a position from FEN
./chesscpp2 -f "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"

# Run Perft test to verify move generation
./chesscpp2 --perft 5

# UCI mode (for chess GUIs/tournaments)
./chesscpp2 --uci

# Start with 8 search threads explicitly
./chesscpp2 --threads 8

# Start UCI mode with 16 search threads
./chesscpp2 --uci --threads 16

# Show help
./chesscpp2 --help
```

### GUI Controls

- **Click** - Select and move pieces
- **A key** - Make AI move
- **R key** - Reset game
- **Close window** - Quit

### Console Mode Commands

- Enter moves in UCI format (e.g., `e2e4`, `e7e8q` for promotion)
- `board` - Display current position
- `fen` - Show FEN string of current position
- `ai` or `a` - Make AI move
- `quit` or `q` - Exit

### Game Animator

Replay saved games from PGN files with animated piece movement and move arrows:

```bash
# Animate the most recent game in games/
python scripts/animate_game.py

# Animate a specific PGN file
python scripts/animate_game.py games/some_game.pgn

# Select game N from a multi-game PGN
python scripts/animate_game.py games/multi.pgn -g 3
```

**Controls:**

| Key | Action |
| --- | ------ |
| Space | Pause / Resume |
| Right / L | Next move |
| Left / H | Previous move |
| Up / K | Speed up |
| Down / J | Slow down |
| R | Restart game |
| F | Flip board |
| G | Next game (multi-game PGN) |
| Q / Escape | Quit |

Quiet moves show green arrows, captures show red. Pieces slide with easing animation, and castling animates both king and rook.

### Syzygy Tablebases

The engine supports Syzygy endgame tablebases for perfect play in positions with 3-5 pieces.

**Setup:**

1. Download tablebase files from [tablebase.sesse.net](http://tablebase.sesse.net/syzygy/3-4-5/)
2. Place `.rtbw` and `.rtbz` files in a `syzygy/` directory

**UCI configuration:**

```text
setoption name SyzygyPath value /path/to/syzygy
```

When the engine reaches a tablebase position, it will report:

```text
info string Tablebase hit: win (DTZ: 15)
```

### Polyglot Opening Books

The engine supports Polyglot opening books (.bin format) for strong opening play.

**Setup:**

1. Download a Polyglot book (e.g., Titans.bin, Human.bin, codekiddy.bin)
2. Place the .bin file in a `books/` directory (or any location)

**UCI configuration:**

```text
setoption name BookPath value /path/to/books/Titans.bin
```

When using a book move, the engine reports:

```text
info string Polyglot book move: e2e4
```

**Recommended books:**

- **Titans.bin** - Strong, balanced play (recommended)
- **Human.bin** - Human-like opening choices
- **codekiddy.bin** - Large, comprehensive book
- **gm2600.bin** - Grandmaster-level openings

## Architecture

### Key Components

1. **Types.h** - Core type definitions (Bitboard, Square, Move, Piece, etc.)
2. **MoveList.h** - Stack-allocated fixed-capacity move lists (`FixedList<T,N>`, zero heap allocation)
3. **Bitboard.h/cpp** - Bitboard operations and pre-computed attack tables
4. **Magic.h/cpp** - Magic bitboard implementation for sliding pieces
5. **Position.h/cpp** - Board position with bitboard representation, incremental eval accumulators (material, PST, phase)
6. **PST.h** - Shared constexpr piece-square tables and material values
7. **MoveGen.h/cpp** - Fast legal move generation with in-place legality filtering
8. **MovePicker.h/cpp** - Staged move generation (TT → captures → killers → quiets)
9. **AI.h/cpp** - Lazy SMP multi-threaded alpha-beta search with shared lockless TT, ThreadData per-thread state, logarithmic LMR, singular extensions, adaptive null move pruning, history malus, PVS, TT prefetching
10. **Eval.h/cpp** - Position evaluation: incremental PST via Position accumulators, tapered eval, pawn hash table, king safety with attack units, mobility
11. **Game.h/cpp** - Game controller and rules
12. **Window.h/cpp** - SDL2 GUI implementation
13. **UCI.h/cpp** - Universal Chess Interface protocol
14. **Polyglot.h/cpp** - Polyglot opening book support
15. **Tablebase.h/cpp** - Syzygy tablebase probing via Fathom
16. **Zobrist.h/cpp** - Zobrist hashing for positions
17. **Logger.h** - Thread-safe logging utility

### Bitboard Advantages

- **Speed** - Bitwise operations are extremely fast
- **Parallelism** - Process multiple squares simultaneously
- **Compact** - All pieces of one type fit in a single 64-bit integer
- **Efficient move generation** - Magic bitboards enable fast sliding piece moves

### Magic Bitboards

Magic bitboards use a technique where:

1. Relevant occupancy bits are extracted from the board
2. A magic number multiplies the occupancy
3. The result is right-shifted to create an index
4. Pre-computed attack tables are looked up using the index

This provides nearly instant rook, bishop, and queen move generation.

## Performance

### Perft Results (Move Generation Speed)

From the starting position with **magic bitboards enabled**:

| Depth | Nodes      | Time     | Speed (Mnodes/s) | Status |
|-------|------------|----------|------------------|--------|
| 1     | 20         | <0.001s  | -                | ✓      |
| 2     | 400        | <0.001s  | -                | ✓      |
| 3     | 8,902      | <0.001s  | -                | ✓      |
| 4     | 197,281    | 0.044s   | 4.5              | ✓      |
| 5     | 4,865,609  | 0.122s   | 39               | ✓      |

**Note**: Stack-allocated `MoveList` provides +11% NPS over `std::vector<Move>`.

## Testing

Run the full test suite (188 tests):

```bash
cd build
ctest
```

Or run individual test suites:

```bash
./test/test_bitboard      # Bitboard operations
./test/test_movegen        # Move generation + checking moves
./test/test_position       # Position make/unmake, SEE, draw detection, incremental eval accumulators
./test/test_polyglot       # Polyglot book handling
./test/test_eval           # Evaluation (material, PST, pawn structure, king safety)
./test/test_ai             # Search (mate detection, TT, time management, Lazy SMP)
./test/test_uci            # UCI protocol compliance
./test/test_game           # Game logic (moves, draws, checkmate, special moves)
```

Verify perft node counts against known values (also runs in CI):

```bash
bash scripts/verify_perft.sh
```

## Improvements Over Original chesscpp

1. **Bitboards** - Orders of magnitude faster than array-based representation
2. **Magic bitboards** - Extremely fast sliding piece move generation (~37 Mnodes/s at perft 5)
3. **Better AI** - Alpha-beta pruning with move ordering
4. **Piece-square tables** - Positional awareness in evaluation
5. **Perft testing** - Built-in move generation verification (all tests passing!)
6. **Modular design** - Clean separation of concerns
7. **Correct move generation** - Fixed diagonal wrapping bug, proper unmake implementation
8. **Complete implementation** - All chess rules including castling, en passant, promotion

## Future Enhancements

Potential improvements:

- [x] Transposition tables for caching positions (implemented - 128MB)
- [x] Iterative deepening (implemented)
- [x] Quiescence search (implemented)
- [x] Null move pruning (implemented)
- [x] Aspiration windows (implemented)
- [x] Late move reductions - LMR (implemented)
- [x] Opening book (implemented - Polyglot .bin format + `book.txt` fallback)
- [x] Endgame tablebases (implemented - Syzygy 3-4-5 piece)
- [x] UCI protocol support (implemented - `--uci` flag)
- [x] Better piece sprites in GUI (implemented)
- [x] Threefold repetition detection (implemented)
- [x] Insufficient material detection (implemented)
- [x] Time controls (implemented - wtime/btime/winc/binc)
- [x] Evaluation extracted to Eval.h/cpp with clean namespace interface
- [x] Search decomposed into named helpers (probeTT, storeTT, tryNullMovePruning, canPrune)
- [x] SEE caching via ScoredMove struct (eliminates redundant SEE in quiescence)
- [x] Efficient checking move generation for quiescence search
- [x] Stack-allocated move lists (`MoveList.h` — `FixedList<T,N>` with zero heap allocation, +11% NPS)
- [x] Comprehensive test suite (188 tests: eval, search, UCI, game logic, move generation, incremental accumulators, Lazy SMP)
- [x] Syzygy tablebase probing during search (WDL probe at depth >= 2)
- [x] Staged move generation via MovePicker (TT → captures → killers → quiets → bad captures)
- [x] Lazy legality checking (skip make/unmake for pruned/cutoff moves)
- [x] Logarithmic LMR table (`log(depth) * log(moveNum)` — reductions up to 5+ at deep nodes)
- [x] Multi-bucket transposition table (4 entries/bucket, packed 10-byte entries, ~2.5x more entries)
- [x] Mate score ply adjustment in TT (correct mate distance at any retrieval ply)
- [x] Singular extensions (extend TT move by 1 ply when significantly better than alternatives)
- [x] History malus (penalize quiet moves that fail to cause cutoffs)
- [x] King safety attack unit counting (quadratic penalty for coordinated piece attacks)
- [x] Pawn hash table (16K entries, ~95% hit rate, caches pawn structure evaluation)
- [x] Adaptive null move pruning (R = 3 + depth/6, eval-boosted)
- [x] Futility pruning fix (no longer incorrectly applied at PV nodes)
- [x] Incremental PST and material tracking (O(1) eval lookups via Position accumulators, shared PST.h)
- [x] TT prefetching (`__builtin_prefetch` on TT bucket before probe, hides memory latency)
- [x] Lazy SMP multithreading (shared lockless TT, per-thread heuristics, depth-skip diversity, best-thread result selection, helper aspiration variation, UCI `Threads` option)
- [ ] NNUE evaluation (neural network-based eval for major strength gain)
- [ ] Endgame-specific knowledge (K+R vs K technique, opposition, pawn endgame rules)

## License

This project is open source. Feel free to use and modify as needed.

## Acknowledgments

- Inspired by the original chesscpp implementation
- Magic bitboard technique from Tord Romstad and others
- Chess programming community resources
