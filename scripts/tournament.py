#!/usr/bin/env python3
"""
UCI Chess Tournament Framework
"""

import subprocess
import shlex
import time
import sys
import os
from dataclasses import dataclass, field
from datetime import datetime
from typing import List, Dict, Optional

try:
    import chess
    HAS_PYTHON_CHESS = True
except ImportError:
    HAS_PYTHON_CHESS = False

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
    move_list: List[str] = field(default_factory=list)

    def to_pgn(self, round_num: int = 1) -> str:
        """Export game as PGN string"""
        lines = []
        date = datetime.now().strftime("%Y.%m.%d")
        lines.append(f'[Event "Engine Tournament"]')
        lines.append(f'[Site "Local"]')
        lines.append(f'[Date "{date}"]')
        lines.append(f'[Round "{round_num}"]')
        lines.append(f'[White "{self.white}"]')
        lines.append(f'[Black "{self.black}"]')
        lines.append(f'[Result "{self.result}"]')
        lines.append(f'[Termination "{self.reason}"]')
        lines.append("")

        # Convert UCI moves to SAN if python-chess available
        if HAS_PYTHON_CHESS:
            board = chess.Board()
            san_parts = []
            for i, uci_move in enumerate(self.move_list):
                try:
                    move = chess.Move.from_uci(uci_move)
                    san = board.san(move)
                    board.push(move)
                    if i % 2 == 0:
                        san_parts.append(f"{i // 2 + 1}. {san}")
                    else:
                        san_parts.append(san)
                except ValueError:
                    san_parts.append(uci_move)
        else:
            san_parts = []
            for i, uci_move in enumerate(self.move_list):
                if i % 2 == 0:
                    san_parts.append(f"{i // 2 + 1}. {uci_move}")
                else:
                    san_parts.append(uci_move)

        # Wrap at ~80 chars
        movetext = ""
        line = ""
        for part in san_parts:
            if len(line) + len(part) + 1 > 80:
                movetext += line + "\n"
                line = part
            else:
                line = f"{line} {part}".strip()
        movetext += f"{line} {self.result}"
        lines.append(movetext.strip())
        lines.append("")
        return "\n".join(lines)


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
            shlex.split(self.path),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
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
        # Send position — prefer FEN to avoid replaying entire move history
        if position_fen == "startpos" and not moves:
            self._send("position startpos")
        elif position_fen != "startpos":
            self._send(f"position fen {position_fen}")
        else:
            # Fallback: startpos + moves (only used if no FEN available)
            moves_str = " moves " + " ".join(moves)
            self._send(f"position startpos{moves_str}")

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
        if self.process.poll() is not None:
            raise RuntimeError(f"Engine '{self.name}' exited with code {self.process.returncode} before receiving '{command}'")
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

    def run_round_robin(self, games_per_pairing: int = 1, depth: int = 6, movetime: Optional[int] = None, pgn_file: Optional[str] = None):
        """Run round-robin tournament. If pgn_file is set, saves all games to that file."""
        if not HAS_PYTHON_CHESS:
            print("WARNING: python-chess not installed — draw adjudication disabled (pip install python-chess)")

        # Start all engines once for the entire tournament
        for engine in self.engines:
            engine.start()

        total_games = len(self.engines) * (len(self.engines) - 1) * games_per_pairing
        game_num = 0

        try:
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
        finally:
            # Stop all engines when tournament is done
            for engine in self.engines:
                engine.stop()

        # Save PGN
        pgn_path = pgn_file or self._default_pgn_path()
        self.save_pgn(pgn_path)

    def _play_game(self, white: Engine, black: Engine, depth: int, movetime: Optional[int]) -> GameResult:
        """Play a single game between two engines (engines must already be started)"""
        white.new_game()
        black.new_game()

        moves = []
        board = chess.Board() if HAS_PYTHON_CHESS else None
        max_moves = 600  # Safety net only — draw rules should trigger first

        try:
            for move_num in range(max_moves):
                current_engine = white if move_num % 2 == 0 else black
                # Send FEN when available (avoids replaying entire game each move)
                current_fen = board.fen() if board is not None else "startpos"
                move = current_engine.get_move(current_fen, moves, depth, movetime)

                if not move or move == "0000" or move == "(none)":
                    # Engine returned no move — use board to distinguish checkmate vs stalemate
                    if board is not None:
                        if board.is_checkmate():
                            winner = black.name if move_num % 2 == 0 else white.name
                            result_str = "0-1" if move_num % 2 == 0 else "1-0"
                            print(f"  Result: {winner} wins by checkmate")
                            return GameResult(white.name, black.name, result_str, len(moves), "checkmate", list(moves))
                        elif board.is_stalemate():
                            print(f"  Result: Draw (stalemate)")
                            return GameResult(white.name, black.name, "1/2-1/2", len(moves), "stalemate", list(moves))

                    # Fallback without python-chess: assume checkmate
                    if move_num % 2 == 0:
                        print(f"  Result: {black.name} wins by checkmate")
                        return GameResult(white.name, black.name, "0-1", len(moves), "checkmate", list(moves))
                    else:
                        print(f"  Result: {white.name} wins by checkmate")
                        return GameResult(white.name, black.name, "1-0", len(moves), "checkmate", list(moves))

                moves.append(move)

                # Update board for draw detection
                if board is not None:
                    try:
                        board.push_uci(move)
                    except ValueError:
                        print(f"  Warning: illegal move '{move}' from {current_engine.name}, ply {len(moves)}")
                        loser = white.name if move_num % 2 == 0 else black.name
                        winner = black.name if move_num % 2 == 0 else white.name
                        result_str = "0-1" if move_num % 2 == 0 else "1-0"
                        print(f"  Result: {winner} wins by forfeit (illegal move)")
                        return GameResult(white.name, black.name, result_str, len(moves), f"illegal move: {move}", list(moves))

                    # --- Draw adjudication ---
                    if board.is_fivefold_repetition():
                        print(f"  Result: Draw (fivefold repetition) at ply {len(moves)}")
                        return GameResult(white.name, black.name, "1/2-1/2", len(moves), "fivefold repetition", list(moves))

                    if board.is_repetition(3):
                        print(f"  Result: Draw (threefold repetition) at ply {len(moves)}")
                        return GameResult(white.name, black.name, "1/2-1/2", len(moves), "threefold repetition", list(moves))

                    if board.is_fifty_moves():
                        print(f"  Result: Draw (50-move rule) at ply {len(moves)}")
                        return GameResult(white.name, black.name, "1/2-1/2", len(moves), "50-move rule", list(moves))

                    if board.is_insufficient_material():
                        print(f"  Result: Draw (insufficient material) at ply {len(moves)}")
                        return GameResult(white.name, black.name, "1/2-1/2", len(moves), "insufficient material", list(moves))

                # Print progress every 10 moves
                if len(moves) % 10 == 0:
                    print(f"  Move {len(moves)}: {move}")

            # Safety net — should rarely reach here with draw adjudication active
            result = GameResult(white.name, black.name, "1/2-1/2", len(moves), "max moves", list(moves))
            print(f"  Result: Draw (max moves)")
            return result

        finally:
            pass  # Engines are managed by the tournament, not per-game

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

        # Draw reason breakdown
        draw_results = [r for r in self.results if r.result == "1/2-1/2"]
        if draw_results:
            reasons = {}
            for r in draw_results:
                reasons[r.reason] = reasons.get(r.reason, 0) + 1
            reason_parts = [f"{reason}: {count}" for reason, count in sorted(reasons.items())]
            print(f"  Draws: {', '.join(reason_parts)}")
        print("=" * 60)


    def _default_pgn_path(self) -> str:
        """Generate a default PGN filename based on engine names and timestamp"""
        names = "_vs_".join(e.name.replace(" ", "-") for e in self.engines[:2])
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        return os.path.join(PROJECT_ROOT, "games", f"{names}_{ts}.pgn")

    def save_pgn(self, path: str):
        """Save all games to a PGN file"""
        if not self.results:
            return
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            for i, result in enumerate(self.results, 1):
                f.write(result.to_pgn(round_num=i))
                f.write("\n")
        print(f"\nPGN saved to {path}")


if __name__ == "__main__":
    print("Tournament framework loaded")
    print("Use: from tournament import Engine, Tournament")
