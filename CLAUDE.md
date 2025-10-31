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
./chesscpp2 -c -d 4      # Play vs AI at depth 4
./chesscpp2 --nogui -c   # Console mode
./chesscpp2 --perft 5    # Run perft test to depth 5
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
- No transposition table (yet)
- No quiescence search (yet)

**Performance by depth:**
- Depth 3: ~0.1s per move (beginner)
- Depth 4: ~1s per move (recommended)
- Depth 5: ~10s per move (advanced)
- Depth 6+: 30s+ per move (expert)

## Component Organization

### Core Engine Files
- [Types.h](inc/Types.h) - Type definitions, constants, move encoding/decoding
- [Bitboard.h/cpp](inc/Bitboard.h) - Bitboard operations, attack lookups
- [Magic.h/cpp](inc/Magic.h) - Magic bitboard initialization and attack generation
- [Position.h/cpp](inc/Position.h) - Board position, make/unmake moves, FEN parsing
- [MoveGen.h/cpp](inc/MoveGen.h) - Legal and pseudo-legal move generation
- [Zobrist.h/cpp](inc/Zobrist.h) - Zobrist hashing for positions
- [AI.h/cpp](inc/AI.h) - Search algorithm with evaluation

### Interface Files
- [Game.h/cpp](inc/Game.h) - Game controller, rules, game state
- [Window.h/cpp](inc/Window.h) - SDL2 GUI implementation
- [main.cpp](src/main.cpp) - Entry point, CLI argument parsing

### Build System
- [CMakeLists.txt](CMakeLists.txt) - Main CMake config (C++20, SDL2 dependencies)
- [test/CMakeLists.txt](test/CMakeLists.txt) - Test config using Google Test

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

1. GUI uses simple colored squares instead of piece sprites
2. No transposition table (positions are re-evaluated)
3. No opening book or endgame tablebases
4. Fullmove counter can desync if unmakeMove logic is incorrect
