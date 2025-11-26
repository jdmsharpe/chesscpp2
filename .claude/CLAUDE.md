# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chess++ with Bitboards is a high-performance chess engine using bitboard representation for fast move generation. The engine achieves ~37 million nodes/second at perft depth 5 using magic bitboards.

## Build Commands

### Standard Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

### Running the Engine

```bash
# From build/ directory
./chesscpp2              # GUI mode (default)
./chesscpp2 -c -d 6      # Play vs AI at depth 6 (default depth)
./chesscpp2 --nogui -c   # Console mode
./chesscpp2 --perft 5    # Run perft test to depth 5
./chesscpp2 --uci        # UCI mode (for GUIs/tournaments)
```

### Testing

```bash
# From build/ directory
ctest                     # Run all tests
./test/test_bitboard      # Run bitboard tests only
./test/test_movegen       # Run move generation tests only
```

## Architecture

### Bitboard-Based Representation

The engine uses 64-bit integers (bitboards) to represent the chess board, where each bit corresponds to one square. This enables parallel operations on multiple squares using fast bitwise operations.

**Key data structures:**

- 12 bitboards total: 6 piece types x 2 colors
- Separate piece array for O(1) `pieceAt()` queries
- All bitboard operations in [Bitboard.h](inc/Bitboard.h) / [Bitboard.cpp](src/Bitboard.cpp)

### Magic Bitboards

Magic bitboards provide O(1) attack generation for sliding pieces (rooks, bishops, queens) using pre-computed lookup tables.

**How it works:**

1. Extract relevant occupancy bits (excluding edges)
2. Multiply by a magic number
3. Right-shift to create an index into attack table
4. Look up pre-computed attacks

**Implementation:** [Magic.h](inc/Magic.h) / [Magic.cpp](src/Magic.cpp)

- Rook attack tables: ~107KB
- Bishop attack tables: ~5KB
- Magic numbers are hard-coded (pre-generated)

### Move Generation Pipeline

Legal move generation follows this pattern:

1. Generate pseudo-legal moves (fast, bitboard-based)
2. Make each move on the position
3. Check if own king is in check (illegal if true)
4. Unmake the move to restore position
5. Only return moves that passed the legality test

**Critical:** `unmakeMove()` in [Position.cpp](src/Position.cpp) must fully reverse ALL aspects of `makeMove()`, including:

- Normal moves: restore piece and captured piece
- Promotions: remove promoted piece, restore pawn
- En passant: restore captured pawn on correct square
- Castling: move both king and rook back

**Common bug:** Incomplete `unmakeMove()` leads to position corruption and segfaults.

### Move Representation

Moves are encoded as 16-bit integers (see [Types.h](inc/Types.h)):

- Bits 0-5: from square (0-63)
- Bits 6-11: to square (0-63)
- Bits 12-13: promotion piece (0=knight, 1=bishop, 2=rook, 3=queen)
- Bits 14-15: move flags (normal, promotion, en passant, castling)

### Edge Wrapping Prevention

**Critical:** Diagonal move generation must check for edge wrapping. Without these checks, bishops/queens can illegally wrap around board edges (e.g., f8 to h1).

Required checks in sliding move generation:

```cpp
if (dir == 9 && fileOf(to) == FILE_A) break;   // NE wrapped
if (dir == 7 && fileOf(to) == FILE_H) break;   // NW wrapped
if (dir == -7 && fileOf(to) == FILE_A) break;  // SW wrapped
if (dir == -9 && fileOf(to) == FILE_H) break;  // SE wrapped
```

### AI Search

Minimax with alpha-beta pruning in [AI.cpp](src/AI.cpp):

- Move ordering: captures first, then promotions, then center control
- Piece-square tables for positional evaluation
- Opening book support (`book.txt`)
- 128MB transposition table with depth-preferred replacement
- Killer move heuristic
- History heuristic
- Internal iterative deepening (IID)
- Quiescence search (captures + checks at leaf nodes)
- Null move pruning (with zugzwang detection)
- Aspiration windows (for iterative deepening)
- Late move reductions (LMR)

**Performance by depth:**

- Depth 3: ~0.1s per move (beginner)
- Depth 4: ~1s per move (intermediate)
- Depth 5: ~10s per move (advanced)
- Depth 6: ~30s per move (default, recommended)
- Depth 7+: 60s+ per move (expert)

### UCI Protocol

The engine supports the Universal Chess Interface (UCI) protocol for integration with chess GUIs and tournament software.

**Implementation:** [UCI.h](inc/UCI.h) / [UCI.cpp](src/UCI.cpp)

- Standard UCI commands: `uci`, `isready`, `position`, `go`, `stop`, `quit`
- Supports FEN position loading and move sequences
- Configurable search depth via `go depth N`
- Time controls: `wtime`, `btime`, `winc`, `binc`, `movestogo`, `movetime`

### Draw Detection

The engine correctly detects all standard draw conditions:

- **50-move rule**: Draw after 100 half-moves without capture or pawn move
- **Threefold repetition**: Draw when same position occurs 3 times
- **Insufficient material**: KvK, KNvK, KBvK, KBvKB (same-color bishops)

### Syzygy Endgame Tablebases

The engine supports Syzygy tablebases for perfect endgame play. Uses the Fathom library.

**Setup:**

1. Download Syzygy tablebases from [syzygy-tables.info](https://syzygy-tables.info/)
2. Place `.rtbw` (WDL) and `.rtbz` (DTZ) files in a directory
3. Configure via UCI: `setoption name SyzygyPath value /path/to/syzygy`

**Implementation:** [Tablebase.h/cpp](inc/Tablebase.h) wrapping [lib/Fathom](lib/Fathom)

- Probes at root for perfect play in tablebase positions
- WDL (Win/Draw/Loss) probing during search
- DTZ (Distance-To-Zero) for optimal move selection
- Supports positions with up to 7 pieces (depending on downloaded tablebases)

## Component Organization

### Core Engine Files

- [Types.h](inc/Types.h) - Type definitions, constants, move encoding/decoding
- [Bitboard.h/cpp](inc/Bitboard.h) - Bitboard operations, attack lookups
- [Magic.h/cpp](inc/Magic.h) - Magic bitboard initialization and attack generation
- [Position.h/cpp](inc/Position.h) - Board position, make/unmake moves, FEN parsing
- [MoveGen.h/cpp](inc/MoveGen.h) - Legal and pseudo-legal move generation
- [Zobrist.h/cpp](inc/Zobrist.h) - Zobrist hashing for positions
- [AI.h/cpp](inc/AI.h) - Search algorithm with evaluation
- [Tablebase.h/cpp](inc/Tablebase.h) - Syzygy tablebase probing (via Fathom)
- [Logger.h](inc/Logger.h) - Thread-safe logging utility

### Interface Files

- [Game.h/cpp](inc/Game.h) - Game controller, rules, game state
- [Window.h/cpp](inc/Window.h) - SDL2 GUI implementation
- [UCI.h/cpp](inc/UCI.h) - UCI protocol handler for GUI/tournament integration
- [main.cpp](src/main.cpp) - Entry point, CLI argument parsing

### Scripts

- [scripts/](scripts/) - Python utilities for testing and tournaments
  - `self_play_test.py` - Engine self-play testing
  - `tournament.py` - Run tournaments between engines
  - `watch_game.py` - Visualize games in progress
  - `diagnose.py` - Engine diagnostics
  - `depth8_vs_stockfish.py` - Benchmark against Stockfish

### Build System

- [CMakeLists.txt](CMakeLists.txt) - Main CMake config (C++20, SDL2 dependencies)
- [test/CMakeLists.txt](test/CMakeLists.txt) - Test config using Google Test

### Design Documents

- [docs/plans/](docs/plans/) - Future feature designs
  - `2025-10-30-lazy-smp-multithreading-design.md` - Lazy SMP parallel search design (not yet implemented)

## Dependencies

- CMake 3.16+
- C++20 compiler (GCC 10+, Clang 10+)
- SDL2 and SDL2_image (for GUI)
- Google Test (auto-fetched for tests)

Install SDL2 on Ubuntu: `sudo apt-get install libsdl2-dev libsdl2-image-dev`

## Testing Strategy

The test suite uses perft (performance test) to verify move generation correctness by counting all legal moves to a given depth.

**Expected perft results from starting position:**

- Depth 1: 20 nodes
- Depth 2: 400 nodes
- Depth 3: 8,902 nodes
- Depth 4: 197,281 nodes
- Depth 5: 4,865,609 nodes

Any deviation indicates a bug in move generation.

## Known Issues

1. Fullmove counter can desync if unmakeMove logic is incorrect
