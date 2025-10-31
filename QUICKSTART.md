# Quick Start Guide

## Building

```bash
cd chesscpp2
mkdir build
cd build
cmake ..
make -j4
```

## Running

### GUI Mode (Default)
```bash
./chesscpp2
```

**Controls:**
- Click squares to select and move pieces
- Press `A` to make the AI move
- Press `R` to reset the game
- Close window to quit

### Play Against AI
```bash
./chesscpp2 -c -d 4    # AI at depth 4 (recommended)
```

### Console Mode
```bash
./chesscpp2 --nogui -c
```

**Commands:**
- Enter moves in UCI format: `e2e4`, `e7e8q` (promotion)
- `ai` or `a` - Make AI move
- `board` - Display current position
- `fen` - Show FEN string
- `quit` or `q` - Exit

### Load a Position
```bash
./chesscpp2 -f "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"
```

### Run Performance Test
```bash
./chesscpp2 --perft 5    # Test move generation to depth 5
```

## AI Difficulty Levels

- Depth 3: Easy (0.1s per move)
- Depth 4: Medium (1s per move) **← Recommended**
- Depth 5: Hard (10s per move)
- Depth 6: Expert (30s+ per move)

## Example Session

```bash
# Start game with AI at depth 4
$ ./chesscpp2 -c -d 4

# Or in console mode
$ ./chesscpp2 --nogui -c -d 4
```

## Troubleshooting

**SDL2 not found:**
```bash
sudo apt-get install libsdl2-dev libsdl2-image-dev  # Ubuntu/Debian
brew install sdl2 sdl2_image                         # macOS
```

**Compilation errors:**
- Make sure you have C++20 compiler (GCC 10+ or Clang 10+)
- Run `cmake --version` (need 3.16+)

**Slow AI:**
- Reduce search depth with `-d 3`
- The first move is slower due to initialization

## Performance Tips

- Use `-d 4` for balanced play (recommended)
- The engine uses magic bitboards for speed (~37 million nodes/second)
- Perft 5 completes in ~0.13 seconds
- AI searches ~3000-7000 nodes per move at depth 4

Enjoy your chess games!
