#!/usr/bin/env python3
"""
Chess++ (Depth 8) vs Stockfish (Skill Level 2)
"""

import sys
from tournament import Engine, Tournament

print("=" * 60)
print("Chess++ Depth-8 vs Stockfish Skill Level 3")
print("=" * 60)
print("Testing Chess++ at depth 8 against Stockfish at skill level 3")
print("=" * 60)

chesscpp = Engine(
    name="Chess++ Depth-8",
    path="./build/chesscpp2 --uci",
    options={"Depth": "8"},
)

stockfish = Engine(
    name="Stockfish Level-3",
    path="./stockfish/stockfish-ubuntu-x86-64-avx2",
    options={"Skill Level": "3"},
)

tournament = Tournament([chesscpp, stockfish])

# Play 6 games (3 as white, 3 as black)
try:
    print("\nPlaying 6 games (alternating colors)...")
    tournament.run_round_robin(games_per_pairing=3, depth=8)
except KeyboardInterrupt:
    print("\n\nTournament interrupted by user")
    sys.exit(1)
