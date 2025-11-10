#!/usr/bin/env python3
"""
Chess++ vs Chess++ - should be roughly 50-50
"""

import sys
from tournament import Engine, Tournament

print("=" * 60)
print("Chess++ Self-Play Test")
print("=" * 60)
print("Testing for color bias - should be close to 50-50")
print("=" * 60)

chesscpp_white = Engine(
    name="Chess++ Depth-4 (White pref)",
    path="../build/chesscpp2 --uci",
    options={"Depth": "4"},
)

chesscpp_black = Engine(
    name="Chess++ Depth-4 (Black pref)",
    path="../build/chesscpp2 --uci",
    options={"Depth": "4"},
)

tournament = Tournament([chesscpp_white, chesscpp_black])

# Play 6 games
try:
    print("\nPlaying 6 games (alternating colors)...")
    tournament.run_round_robin(games_per_pairing=6, depth=4)
except KeyboardInterrupt:
    print("\n\nTournament interrupted by user")
    sys.exit(1)
