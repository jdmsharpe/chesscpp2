#!/usr/bin/env python3
"""
Chess++ (Depth 12) vs Stockfish (Skill Level 8)
"""

import sys

from tournament import PROJECT_ROOT, Engine, Tournament

print("=" * 60)
print("Chess++ Depth-12 vs Stockfish Skill Level 8")
print("=" * 60)
print("Testing Chess++ at depth 12 against Stockfish at skill level 8")
print("=" * 60)

chesscpp = Engine(
    name="Chess++ Depth-12",
    path=f"{PROJECT_ROOT}/build/chesscpp2 --uci",
    options={"Depth": "12", "Hash": "64"},
)

stockfish = Engine(
    name="Stockfish Level-8",
    path=f"{PROJECT_ROOT}/stockfish/stockfish-ubuntu-x86-64-avx2",
    options={"Skill Level": "8", "Hash": "64"},
)

tournament = Tournament([chesscpp, stockfish])

# Play 100 games (50 as white, 50 as black)
try:
    print("\nPlaying 100 games (alternating colors)...")
    tournament.run_round_robin(games_per_pairing=50, depth=12)
except KeyboardInterrupt:
    print("\n\nTournament interrupted by user")
    sys.exit(1)
