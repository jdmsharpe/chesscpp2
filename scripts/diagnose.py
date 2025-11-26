#!/usr/bin/env python3
"""
Diagnostic tool to see what Chess++ is thinking
"""

import subprocess
import time

def send_uci_command(proc, command):
    """Send command and return response lines"""
    proc.stdin.write(command + "\n")
    proc.stdin.flush()

def get_uci_response(proc, wait_for):
    """Read until we see the expected line"""
    lines = []
    while True:
        line = proc.stdout.readline().strip()
        lines.append(line)
        if line.startswith(wait_for):
            break
    return lines

def test_position(fen, moves_str="", depth=5):
    """Test what the engine thinks about a position"""
    print("=" * 70)
    print(f"Testing position: {fen}")
    if moves_str:
        print(f"After moves: {moves_str}")
    print("=" * 70)

    # Start engine
    proc = subprocess.Popen(
        "../build/chesscpp2 --uci",
        shell=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    # Initialize
    send_uci_command(proc, "uci")
    get_uci_response(proc, "uciok")

    send_uci_command(proc, "isready")
    get_uci_response(proc, "readyok")

    # Set position
    if moves_str:
        send_uci_command(proc, f"position {fen} moves {moves_str}")
    else:
        send_uci_command(proc, f"position {fen}")

    # Get best move
    send_uci_command(proc, f"go depth {depth}")
    response = get_uci_response(proc, "bestmove")

    for line in response:
        if line.startswith("bestmove"):
            bestmove = line.split()[1]
            print(f"Best move (depth {depth}): {bestmove}")
            break

    send_uci_command(proc, "quit")
    proc.wait(timeout=2)
    print()

# Test 1: Starting position
print("\n*** TEST 1: Starting position ***")
test_position("startpos", "", depth=5)

# Test 2: After 1.e4
print("\n*** TEST 2: After 1.e4 (Black to move) ***")
test_position("startpos", "e2e4", depth=5)

# Test 3: Simple tactic - can we capture a free piece?
print("\n*** TEST 3: Free knight on e5 (White to move) ***")
test_position("fen rnbqkb1r/pppp1ppp/5n2/4N3/4P3/8/PPPP1PPP/RNBQKB1R w KQkq - 0 1", "", depth=5)

# Test 4: Can we avoid hanging our queen?
print("\n*** TEST 4: Queen hanging on d4 - can we move it? (White to move) ***")
test_position("fen rnbqkbnr/pppp1ppp/8/4p3/3Q4/8/PPPP1PPP/RNB1KBNR w KQkq - 0 1", "", depth=5)

# Test 5: Checkmate in 1
print("\n*** TEST 5: Checkmate in 1 with Qh5# (White to move) ***")
test_position("fen rnb1kbnr/pppp1ppp/8/4p2q/4PP2/8/PPPP2PP/RNBQKBNR w KQkq - 0 1", "", depth=5)

# Test 6: Simple king safety - can we castle?
print("\n*** TEST 6: Should we castle? (White to move) ***")
test_position("fen rnbqk2r/pppp1ppp/5n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1", "", depth=5)

print("\n" + "=" * 70)
print("DIAGNOSIS COMPLETE")
print("=" * 70)
