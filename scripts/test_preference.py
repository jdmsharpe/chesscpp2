#!/usr/bin/env python3
"""
Test if the engine prefers obviously good moves
"""

import subprocess

def test_move(fen, depth, description):
    """Test what move the engine chooses"""
    proc = subprocess.Popen(
        "../build/chesscpp2 --uci",
        shell=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    proc.stdin.write("uci\n")
    proc.stdin.flush()
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("uciok"):
            break

    proc.stdin.write("isready\n")
    proc.stdin.flush()
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("readyok"):
            break

    proc.stdin.write(f"position fen {fen}\n")
    proc.stdin.flush()

    proc.stdin.write(f"go depth {depth}\n")
    proc.stdin.flush()

    bestmove = None
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("bestmove"):
            bestmove = line.split()[1]
            break

    proc.stdin.write("quit\n")
    proc.stdin.flush()
    proc.wait(timeout=2)

    print(f"{description}")
    print(f"  FEN: {fen}")
    print(f"  Best move (depth {depth}): {bestmove}")
    print()

    return bestmove

print("="*70)
print("TESTING MOVE PREFERENCES")
print("="*70)
print()

# Test 1: Can we capture a free queen?
test_move(
    "rnb1kbnr/pppppppp/8/8/8/3q4/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    5,
    "Test 1: Free queen on d3 - should capture it"
)

# Test 2: Should we save our own queen from being captured?
test_move(
    "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPPQPPP/RNB1KBNR w KQkq - 0 1",
    5,
    "Test 2: Our queen on e2 can be taken by e5 - should move it"
)

# Test 3: Do we see a back rank mate in 1?
test_move(
    "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1",
    5,
    "Test 3: Back rank mate with Re8# - should find it"
)

# Test 4: Do we avoid moving into check?
test_move(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1",
    3,
    "Test 4: King on e1, should NOT move to d1 (would be in check from queen)"
)

print("="*70)
