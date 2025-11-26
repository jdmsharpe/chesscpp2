# Chess++ with Bitboards

An improved chess engine implementation using bitboards for fast move generation and position evaluation.

## Features

### Core Engine

- **Bitboard representation** - 64-bit integers for efficient board representation
- **Magic bitboards** - Ultra-fast sliding piece (rook, bishop, queen) move generation
- **Fast move generation** - Optimized legal move generation using bit operations
- **AI with alpha-beta pruning** - Minimax search with move ordering and pruning
- **Piece-square tables** - Positional evaluation for better play
- **FEN support** - Load and save positions in Forsyth-Edwards Notation

### User Interface

- **SDL2 GUI** - Graphical chess board with mouse input
- **Console mode** - Text-based interface for terminal play
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
cmake ..
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

## Usage

### Basic Commands

```bash
# Run with GUI (default)
./chesscpp2

# Play against AI
./chesscpp2 -c

# Set AI search depth (default is 4)
./chesscpp2 -c -d 5

# Run in console mode (no GUI)
./chesscpp2 --nogui

# Load a position from FEN
./chesscpp2 -f "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"

# Run Perft test to verify move generation
./chesscpp2 --perft 5

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

## Architecture

### Key Components

1. **Types.h** - Core type definitions (Bitboard, Square, Move, Piece, etc.)
2. **Bitboard.h/cpp** - Bitboard operations and pre-computed attack tables
3. **Magic.h/cpp** - Magic bitboard implementation for sliding pieces
4. **Position.h/cpp** - Board position with bitboard representation
5. **MoveGen.h/cpp** - Fast legal move generation
6. **AI.h/cpp** - Minimax with alpha-beta pruning
7. **Game.h/cpp** - Game controller and rules
8. **Window.h/cpp** - SDL2 GUI implementation

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
| 5     | 4,865,609  | 0.132s   | 37               | ✓      |

**Note**: All perft results are correct and verified!

### AI Performance

- Depth 3: ~0.1 seconds per move (beginner level)
- Depth 4: ~1 second per move (intermediate level)
- Depth 5: ~10 seconds per move (advanced level)
- Depth 6+: 30+ seconds per move (expert level)

**Magic bitboards provide ~10x speedup over array-based move generation!**

## Testing

Run the test suite:

```bash
cd build
ctest
```

Or run individual tests:

```bash
./test/test_bitboard
./test/test_movegen
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
- [x] Opening book (implemented - `book.txt`)
- [ ] Endgame tablebases
- [x] UCI protocol support (implemented - `--uci` flag)
- [x] Better piece sprites in GUI (implemented)
- [ ] Threefold repetition detection
- [ ] Insufficient material detection
- [ ] Time controls

## License

This project is open source. Feel free to use and modify as needed.

## Acknowledgments

- Inspired by the original chesscpp implementation
- Magic bitboard technique from Tord Romstad and others
- Chess programming community resources
