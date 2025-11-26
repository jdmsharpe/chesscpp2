#!/usr/bin/env python3
"""
Watch a single game move-by-move and output in PGN format
"""

import subprocess
import time
import os
from datetime import datetime

try:
    import chess
    import chess.pgn
    HAS_CHESS = True
except ImportError:
    HAS_CHESS = False
    print("Warning: python-chess not installed. Install with: pip install chess")
    print("Falling back to UCI notation output\n")

# Default paths (relative to project root)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_BOOK_PATH = os.path.join(PROJECT_ROOT, "books", "Titans.bin")

def create_engine(path, options=None):
    """Create and initialize a UCI engine"""
    proc = subprocess.Popen(
        path,
        shell=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    assert proc.stdin is not None and proc.stdout is not None

    # Initialize
    proc.stdin.write("uci\n")
    proc.stdin.flush()
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("uciok"):
            break

    # Set options
    if options:
        for key, value in options.items():
            proc.stdin.write(f"setoption name {key} value {value}\n")
            proc.stdin.flush()

    proc.stdin.write("isready\n")
    proc.stdin.flush()
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("readyok"):
            break

    return proc

def get_move(proc, position, moves, depth):
    """Get best move from engine"""
    # Send position
    if moves:
        moves_str = " moves " + " ".join(moves)
    else:
        moves_str = ""

    proc.stdin.write(f"position {position}{moves_str}\n")
    proc.stdin.flush()

    # Request move
    proc.stdin.write(f"go depth {depth}\n")
    proc.stdin.flush()

    # Wait for bestmove
    while True:
        line = proc.stdout.readline().strip()
        if line.startswith("bestmove"):
            return line.split()[1]

def uci_to_san(board, uci_move):
    """Convert UCI move to SAN if python-chess is available"""
    if HAS_CHESS:
        try:
            move = chess.Move.from_uci(uci_move)  # type: ignore[possibly-undefined]
            san = board.san(move)
            board.push(move)
            return san
        except:
            return uci_move
    else:
        return uci_move

def play_game():
    """Play one game and output in PGN format"""
    # PGN Header
    date = datetime.now()
    print('[Event "Chess++ vs Stockfish"]')
    print('[Site "Local Engine Match"]')
    print(f'[Date "{date.strftime("%Y.%m.%d")}"]')
    print('[Round "1"]')
    print('[White "Chess++ v2.0"]')
    print('[Black "Stockfish Level-1"]')
    print('[Result "*"]')
    print()

    # Create engines with Polyglot book if available
    chesscpp_options = {"Depth": "8"}
    if os.path.isfile(DEFAULT_BOOK_PATH):
        chesscpp_options["BookPath"] = DEFAULT_BOOK_PATH
    chesscpp = create_engine("./build/chesscpp2 --uci", chesscpp_options)
    stockfish = create_engine("./stockfish/stockfish-ubuntu-x86-64-avx2", {"Skill Level": "1"})
    assert chesscpp.stdin is not None and stockfish.stdin is not None

    # Start game
    chesscpp.stdin.write("ucinewgame\n")
    chesscpp.stdin.flush()
    stockfish.stdin.write("ucinewgame\n")
    stockfish.stdin.flush()

    moves = []
    max_moves = 200
    result = "*"

    if HAS_CHESS:
        board = chess.Board()  # type: ignore[possibly-undefined]

    # Play the game
    for move_num in range(max_moves):
        # White's turn (Chess++)
        if move_num % 2 == 0:
            uci_move = get_move(chesscpp, "startpos", moves, 5)
            if uci_move == "0000" or uci_move == "(none)" or not uci_move:
                result = "0-1"  # Black wins (White has no legal moves)
                break
            moves.append(uci_move)
        # Black's turn (Stockfish)
        else:
            uci_move = get_move(stockfish, "startpos", moves, 5)
            if uci_move == "0000" or uci_move == "(none)" or not uci_move:
                result = "1-0"  # White wins (Black has no legal moves)
                break
            moves.append(uci_move)

        # Check for draws
        if len(moves) >= 150:
            result = "1/2-1/2"
            break

    # Output movetext in PGN format
    if HAS_CHESS:
        board = chess.Board()  # type: ignore[possibly-undefined]
        output = []
        for i, uci_move in enumerate(moves):
            try:
                move = chess.Move.from_uci(uci_move)  # type: ignore[possibly-undefined]
                san = board.san(move)
                board.push(move)

                if i % 2 == 0:
                    output.append(f"{i//2 + 1}. {san}")
                else:
                    output[-1] += f" {san}"
            except:
                # Fallback to UCI notation
                if i % 2 == 0:
                    output.append(f"{i//2 + 1}. {uci_move}")
                else:
                    output[-1] += f" {uci_move}"

        print(" ".join(output))
    else:
        # Fallback: output UCI moves in PGN-like format
        output = []
        for i, uci_move in enumerate(moves):
            if i % 2 == 0:
                output.append(f"{i//2 + 1}. {uci_move}")
            else:
                output[-1] += f" {uci_move}"
        print(" ".join(output))

    print(f" {result}")
    print()

    # Cleanup
    chesscpp.stdin.write("quit\n")
    chesscpp.stdin.flush()
    stockfish.stdin.write("quit\n")
    stockfish.stdin.flush()
    chesscpp.wait(timeout=2)
    stockfish.wait(timeout=2)

if __name__ == "__main__":
    play_game()
