#!/usr/bin/env python3
"""
UCI Chess Tournament Framework
"""

import subprocess
import time
import sys
import os
from dataclasses import dataclass
from typing import List, Dict, Optional

# Default paths (relative to project root)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_SYZYGY_PATH = os.path.join(PROJECT_ROOT, "syzygy")
DEFAULT_BOOK_PATH = os.path.join(PROJECT_ROOT, "books", "Titans.bin")


@dataclass
class GameResult:
    white: str
    black: str
    result: str  # "1-0", "0-1", "1/2-1/2"
    moves: int
    reason: str


class Engine:
    """Represents a UCI chess engine"""

    def __init__(self, name: str, path: str, options: Optional[Dict[str, str]] = None, use_syzygy: bool = True, use_book: bool = True):
        self.name = name
        self.path = path
        self.options = options or {}
        self.process = None

        # Auto-enable Syzygy tablebases if available and not already set
        if use_syzygy and "SyzygyPath" not in self.options:
            if os.path.isdir(DEFAULT_SYZYGY_PATH):
                self.options["SyzygyPath"] = DEFAULT_SYZYGY_PATH

        # Auto-enable Polyglot opening book if available and not already set
        if use_book and "BookPath" not in self.options:
            if os.path.isfile(DEFAULT_BOOK_PATH):
                self.options["BookPath"] = DEFAULT_BOOK_PATH

    def start(self):
        """Start the engine process"""
        self.process = subprocess.Popen(
            self.path,
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        assert self.process.stdin is not None and self.process.stdout is not None

        # Initialize UCI
        self._send("uci")
        self._wait_for("uciok")

        # Set options
        for key, value in self.options.items():
            self._send(f"setoption name {key} value {value}")

        self._send("isready")
        self._wait_for("readyok")

    def stop(self):
        """Stop the engine process"""
        if self.process:
            self._send("quit")
            self.process.wait(timeout=2)
            self.process = None

    def new_game(self):
        """Start a new game"""
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def get_move(self, position_fen: str, moves: List[str], depth: int = 6, movetime: Optional[int] = None) -> Optional[str]:
        """Get best move for current position"""
        # Send position
        if moves:
            moves_str = " moves " + " ".join(moves)
        else:
            moves_str = ""

        if position_fen == "startpos":
            self._send(f"position startpos{moves_str}")
        else:
            self._send(f"position fen {position_fen}{moves_str}")

        # Request move
        if movetime:
            self._send(f"go movetime {movetime}")
        else:
            self._send(f"go depth {depth}")

        # Wait for best move
        bestmove = self._wait_for("bestmove")
        if bestmove:
            parts = bestmove.split()
            if len(parts) >= 2:
                return parts[1]

        return None

    def _send(self, command: str):
        """Send command to engine"""
        assert self.process is not None and self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def _wait_for(self, expected: str, timeout: float = 30.0) -> Optional[str]:
        """Wait for expected response from engine"""
        assert self.process is not None and self.process.stdout is not None
        start_time = time.time()
        while time.time() - start_time < timeout:
            line = self.process.stdout.readline().strip()
            if line.startswith(expected):
                return line
        return None


class Tournament:
    """Manages tournament between multiple engines"""

    def __init__(self, engines: List[Engine]):
        self.engines = engines
        self.results: List[GameResult] = []
        self.scores: Dict[str, float] = {e.name: 0.0 for e in engines}

    def run_round_robin(self, games_per_pairing: int = 1, depth: int = 6, movetime: Optional[int] = None):
        """Run round-robin tournament"""
        total_games = len(self.engines) * (len(self.engines) - 1) * games_per_pairing
        game_num = 0

        for i, engine1 in enumerate(self.engines):
            for j, engine2 in enumerate(self.engines):
                if i == j:
                    continue

                for game in range(games_per_pairing):
                    game_num += 1
                    # Alternate colors
                    if game % 2 == 0:
                        white, black = engine1, engine2
                    else:
                        white, black = engine2, engine1

                    print(f"\n[Game {game_num}/{total_games}] {white.name} (White) vs {black.name} (Black)")
                    result = self._play_game(white, black, depth, movetime)
                    self.results.append(result)
                    self._update_scores(result)
                    self._print_standings()

    def _play_game(self, white: Engine, black: Engine, depth: int, movetime: Optional[int]) -> GameResult:
        """Play a single game between two engines"""
        # Start engines
        white.start()
        black.start()

        white.new_game()
        black.new_game()

        position = "startpos"
        moves = []
        max_moves = 300  # Draw after 300 moves

        try:
            for move_num in range(max_moves):
                # White's turn
                current_engine = white if move_num % 2 == 0 else black
                move = current_engine.get_move(position, moves, depth, movetime)

                if not move or move == "0000" or move == "(none)":
                    # No legal moves - game over
                    if move_num % 2 == 0:
                        # White is in checkmate/stalemate
                        result = GameResult(white.name, black.name, "0-1", len(moves), "checkmate")
                        print(f"  Result: {black.name} wins by checkmate")
                    else:
                        # Black is in checkmate/stalemate
                        result = GameResult(white.name, black.name, "1-0", len(moves), "checkmate")
                        print(f"  Result: {white.name} wins by checkmate")
                    return result

                moves.append(move)

                # Print progress every 10 moves
                if len(moves) % 10 == 0:
                    print(f"  Move {len(moves)}: {move}")

            # Max moves reached - draw
            result = GameResult(white.name, black.name, "1/2-1/2", len(moves), "max moves")
            print(f"  Result: Draw (max moves)")
            return result

        finally:
            white.stop()
            black.stop()

    def _update_scores(self, result: GameResult):
        """Update tournament scores"""
        if result.result == "1-0":
            self.scores[result.white] += 1.0
        elif result.result == "0-1":
            self.scores[result.black] += 1.0
        elif result.result == "1/2-1/2":
            self.scores[result.white] += 0.5
            self.scores[result.black] += 0.5

    def _print_standings(self):
        """Print current tournament standings"""
        print("\n" + "=" * 60)
        print("STANDINGS")
        print("=" * 60)
        sorted_scores = sorted(self.scores.items(), key=lambda x: x[1], reverse=True)

        for name, score in sorted_scores:
            games_played = sum(1 for r in self.results if r.white == name or r.black == name)
            wins = sum(1 for r in self.results if (r.white == name and r.result == "1-0") or (r.black == name and r.result == "0-1"))
            draws = sum(1 for r in self.results if (r.white == name or r.black == name) and r.result == "1/2-1/2")
            losses = games_played - wins - draws

            print(f"{name:40s} {score:5.1f} (+{wins} ={draws} -{losses})")
        print("=" * 60)


if __name__ == "__main__":
    print("Tournament framework loaded")
    print("Use: from tournament import Engine, Tournament")
