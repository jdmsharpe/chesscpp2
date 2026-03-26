#!/usr/bin/env python3
"""
Chess++ vs Chess++ - should be roughly 50-50
"""

import sys

from tournament import PROJECT_ROOT, Engine, Tournament

print("=" * 60)
print("Chess++ Self-Play Test")
print("=" * 60)
print("Testing for color bias - should be close to 50-50")
print("=" * 60)

chesscpp_white = Engine(
    name="Chess++ Depth-10 (White pref)",
    path=f"{PROJECT_ROOT}/build/chesscpp2 --uci",
    options={"Depth": "10", "Hash": "64"},
)

chesscpp_black = Engine(
    name="Chess++ Depth-10 (Black pref)",
    path=f"{PROJECT_ROOT}/build/chesscpp2 --uci",
    options={"Depth": "10", "Hash": "64"},
)

tournament = Tournament([chesscpp_white, chesscpp_black])

# Play 10 games (5 as white, 5 as black)
try:
    print("\nPlaying 10 games (alternating colors)...")
    tournament.run_round_robin(games_per_pairing=5, depth=10)
except KeyboardInterrupt:
    print("\n\nTournament interrupted by user")
    sys.exit(1)
