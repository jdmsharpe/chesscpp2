#!/usr/bin/env python3
"""
Animate chess games from PGN files with sliding pieces and move arrows.

Usage:
    python scripts/animate_game.py                        # Most recent game
    python scripts/animate_game.py games/some_game.pgn    # Specific file
    python scripts/animate_game.py games/multi.pgn -g 3   # 3rd game in file

Controls:
    Space       Pause / Resume
    Right / L   Next move
    Left  / H   Previous move
    Up    / K   Speed up
    Down  / J   Slow down
    R           Restart game
    F           Flip board
    G           Next game (multi-game PGN)
    Q / Escape  Quit
"""

import argparse
import math
import os
import sys

import chess
import chess.pgn
import pygame

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
PIECES_PNG = os.path.join(PROJECT_ROOT, "inc", "pieces.png")
GAMES_DIR = os.path.join(PROJECT_ROOT, "games")

# ---------------------------------------------------------------------------
# Colors — matching Window.cpp board theme
# ---------------------------------------------------------------------------
LIGHT_SQUARE = (240, 217, 181)
DARK_SQUARE = (181, 136, 99)
BG_COLOR = (30, 30, 30)
TEXT_COLOR = (220, 220, 220)
MUTED_TEXT = (140, 140, 140)

# Square highlights (RGBA on transparent overlay)
HIGHLIGHT_FROM_QUIET = (100, 180, 100, 80)
HIGHLIGHT_TO_QUIET = (100, 180, 100, 110)
HIGHLIGHT_FROM_CAPTURE = (210, 90, 90, 80)
HIGHLIGHT_TO_CAPTURE = (210, 90, 90, 110)
CHECK_HIGHLIGHT = (240, 50, 50, 120)

# Arrow colors (RGB — alpha handled separately)
ARROW_QUIET_COLOR = (80, 200, 80)
ARROW_CAPTURE_COLOR = (220, 80, 80)
ARROW_MAX_ALPHA = 140

# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------
SQUARE_SIZE = 80
BOARD_PX = SQUARE_SIZE * 8
INFO_HEIGHT = 120
WIN_W = BOARD_PX
WIN_H = BOARD_PX + INFO_HEIGHT

# ---------------------------------------------------------------------------
# Animation timing (ms)
# ---------------------------------------------------------------------------
SLIDE_MS = 280
ARROW_FADE_IN = 180
ARROW_LINGER = 500
ARROW_FADE_OUT = 250
DEFAULT_DELAY_MS = 1200


# ===================================================================
# Sprite loader
# ===================================================================
class PieceSprites:
    """Slice the sprite sheet into per-piece surfaces."""

    ORDER = [chess.KING, chess.QUEEN, chess.BISHOP, chess.KNIGHT, chess.ROOK, chess.PAWN]

    def __init__(self, path: str, size: int):
        sheet = pygame.image.load(path).convert_alpha()
        sw, sh = sheet.get_size()
        pw, ph = sw // 6, sh // 2
        self._map: dict[tuple[int, bool], pygame.Surface] = {}
        for color in (chess.WHITE, chess.BLACK):
            row = 0 if color == chess.WHITE else 1
            for col, pt in enumerate(self.ORDER):
                src = pygame.Rect(col * pw, row * ph, pw, ph)
                sprite = sheet.subsurface(src)
                self._map[(pt, color)] = pygame.transform.smoothscale(sprite, (size, size))

    def get(self, piece: chess.Piece) -> pygame.Surface | None:
        return self._map.get((piece.piece_type, piece.color))


# ===================================================================
# Drawing helpers
# ===================================================================
def sq_center(sq: int, flipped: bool) -> tuple[int, int]:
    f, r = chess.square_file(sq), chess.square_rank(sq)
    if flipped:
        return (7 - f) * SQUARE_SIZE + SQUARE_SIZE // 2, r * SQUARE_SIZE + SQUARE_SIZE // 2
    return f * SQUARE_SIZE + SQUARE_SIZE // 2, (7 - r) * SQUARE_SIZE + SQUARE_SIZE // 2


def sq_rect(sq: int, flipped: bool) -> pygame.Rect:
    f, r = chess.square_file(sq), chess.square_rank(sq)
    if flipped:
        return pygame.Rect((7 - f) * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE)
    return pygame.Rect(f * SQUARE_SIZE, (7 - r) * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE)


def draw_arrow(
    surface: pygame.Surface,
    start: tuple[int, int],
    end: tuple[int, int],
    color: tuple[int, int, int],
    alpha: int,
    thickness: int = 8,
) -> None:
    """Thick translucent arrow with triangular head."""
    overlay = pygame.Surface(surface.get_size(), pygame.SRCALPHA)
    a = max(0, min(255, alpha))
    sx, sy = start
    ex, ey = end
    dx, dy = ex - sx, ey - sy
    dist = math.hypot(dx, dy)
    if dist < 1:
        return
    ux, uy = dx / dist, dy / dist
    px, py = -uy, ux  # perpendicular

    head_len = min(thickness * 3, dist * 0.3)
    head_w = thickness * 2.2
    shaft_ex = ex - ux * head_len
    shaft_ey = ey - uy * head_len
    half = thickness / 2

    shaft = [
        (sx + px * half, sy + py * half),
        (shaft_ex + px * half, shaft_ey + py * half),
        (shaft_ex - px * half, shaft_ey - py * half),
        (sx - px * half, sy - py * half),
    ]
    head = [
        (ex, ey),
        (shaft_ex + px * head_w, shaft_ey + py * head_w),
        (shaft_ex - px * head_w, shaft_ey - py * head_w),
    ]
    rgba = (*color, a)
    pygame.draw.polygon(overlay, rgba, shaft)
    pygame.draw.polygon(overlay, rgba, head)
    surface.blit(overlay, (0, 0))


# ===================================================================
# Main animator
# ===================================================================
class GameAnimator:
    def __init__(self, pgn_path: str, game_index: int = 0):
        os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
        pygame.init()
        pygame.display.set_caption("Chess++ Game Viewer")
        self.screen = pygame.display.set_mode((WIN_W, WIN_H))
        self.clock = pygame.time.Clock()

        self.sprites = PieceSprites(PIECES_PNG, SQUARE_SIZE)
        self.font = pygame.font.SysFont("monospace", 17, bold=True)
        self.small_font = pygame.font.SysFont("monospace", 13)
        self.coord_font = pygame.font.SysFont("monospace", 11)

        # Load all games from PGN
        self.pgn_path = pgn_path
        self.games = self._load_games(pgn_path)
        self.game_idx = min(game_index, len(self.games) - 1)
        self._init_game()

        self.flipped = False
        self.paused = False
        self.delay = DEFAULT_DELAY_MS
        self.last_advance = pygame.time.get_ticks()

    # ---- data loading ----

    @staticmethod
    def _load_games(path: str) -> list[chess.pgn.Game]:
        games: list[chess.pgn.Game] = []
        with open(path) as fh:
            while True:
                g = chess.pgn.read_game(fh)
                if g is None:
                    break
                games.append(g)
        if not games:
            sys.exit(f"No games in {path}")
        return games

    def _init_game(self) -> None:
        game = self.games[self.game_idx]
        self.headers = dict(game.headers)
        self.moves = list(game.mainline_moves())
        # Pre-compute every board state for instant random access
        self.boards: list[chess.Board] = [game.board().copy()]
        b = game.board()
        for m in self.moves:
            b.push(m)
            self.boards.append(b.copy())
        self.move_idx = -1  # -1 = start position
        self._clear_anim()

    def _clear_anim(self) -> None:
        self.sliding = False
        self.slide_start = 0
        self.slide_piece: chess.Piece | None = None
        self.slide_from = 0
        self.slide_to = 0
        self.slide_is_capture = False
        # Rook castle co-slide
        self.slide_rook: chess.Piece | None = None
        self.rook_from = 0
        self.rook_to = 0
        # En passant captured pawn square
        self.ep_capture_sq: int | None = None
        # Arrow
        self.arrow_on = False
        self.arrow_start = 0
        self.arrow_from = 0
        self.arrow_to = 0
        self.arrow_capture = False

    # ---- board at current index ----

    @property
    def board(self) -> chess.Board:
        return self.boards[max(0, self.move_idx + 1)]

    # ---- move control ----

    def _advance(self) -> None:
        if self.move_idx >= len(self.moves) - 1:
            return
        self.move_idx += 1
        move = self.moves[self.move_idx]
        pre = self.boards[self.move_idx]  # board before this move

        self.sliding = True
        self.slide_start = pygame.time.get_ticks()
        self.slide_piece = pre.piece_at(move.from_square)
        self.slide_from = move.from_square
        self.slide_to = move.to_square
        self.slide_is_capture = pre.is_capture(move)

        # Castle: also slide the rook
        self.slide_rook = None
        if pre.is_castling(move):
            # Determine rook squares from the post-move board
            rook_from, rook_to = self._castle_rook_squares(move, pre)
            self.slide_rook = pre.piece_at(rook_from)
            self.rook_from = rook_from
            self.rook_to = rook_to

        # En passant: track the captured pawn square so we can hide it during slide
        self.ep_capture_sq = None
        if pre.is_en_passant(move):
            # Captured pawn is on the same file as destination, same rank as source
            self.ep_capture_sq = chess.square(
                chess.square_file(move.to_square), chess.square_rank(move.from_square)
            )

        # Start arrow
        self.arrow_on = True
        self.arrow_start = pygame.time.get_ticks()
        self.arrow_from = move.from_square
        self.arrow_to = move.to_square
        self.arrow_capture = self.slide_is_capture

    @staticmethod
    def _castle_rook_squares(move: chess.Move, board: chess.Board) -> tuple[int, int]:
        king_to_file = chess.square_file(move.to_square)
        rank = chess.square_rank(move.from_square)
        if king_to_file == 6:  # kingside
            return chess.square(7, rank), chess.square(5, rank)
        else:  # queenside
            return chess.square(0, rank), chess.square(3, rank)

    def _retreat(self) -> None:
        if self.move_idx < 0:
            return
        self.move_idx -= 1
        self._clear_anim()

    # ---- arrow alpha ----

    def _arrow_alpha(self) -> int:
        if not self.arrow_on:
            return 0
        t = pygame.time.get_ticks() - self.arrow_start
        total = ARROW_FADE_IN + ARROW_LINGER + ARROW_FADE_OUT
        if t < ARROW_FADE_IN:
            return int(ARROW_MAX_ALPHA * t / ARROW_FADE_IN)
        if t < ARROW_FADE_IN + ARROW_LINGER:
            return ARROW_MAX_ALPHA
        if t < total:
            return int(ARROW_MAX_ALPHA * (1 - (t - ARROW_FADE_IN - ARROW_LINGER) / ARROW_FADE_OUT))
        self.arrow_on = False
        return 0

    # ---- rendering ----

    def _draw_board(self) -> None:
        for sq in chess.SQUARES:
            f, r = chess.square_file(sq), chess.square_rank(sq)
            color = LIGHT_SQUARE if (f + r) % 2 == 0 else DARK_SQUARE
            pygame.draw.rect(self.screen, color, sq_rect(sq, self.flipped))

    def _draw_coords(self) -> None:
        files = "abcdefgh"
        for i in range(8):
            fi = 7 - i if self.flipped else i
            ri = i + 1 if self.flipped else 8 - i
            # Bottom-right of each edge square
            fc = self.coord_font.render(files[fi], True, MUTED_TEXT)
            self.screen.blit(fc, (i * SQUARE_SIZE + SQUARE_SIZE - 11, BOARD_PX - 14))
            rc = self.coord_font.render(str(ri), True, MUTED_TEXT)
            self.screen.blit(rc, (2, i * SQUARE_SIZE + 1))

    def _draw_highlights(self) -> None:
        alpha = self._arrow_alpha()
        if alpha <= 0:
            return
        scale = alpha / ARROW_MAX_ALPHA
        overlay = pygame.Surface((SQUARE_SIZE, SQUARE_SIZE), pygame.SRCALPHA)

        from_hl = HIGHLIGHT_FROM_CAPTURE if self.arrow_capture else HIGHLIGHT_FROM_QUIET
        to_hl = HIGHLIGHT_TO_CAPTURE if self.arrow_capture else HIGHLIGHT_TO_QUIET

        r, g, b, a = from_hl
        overlay.fill((r, g, b, int(a * scale)))
        self.screen.blit(overlay, sq_rect(self.arrow_from, self.flipped))

        r, g, b, a = to_hl
        overlay.fill((r, g, b, int(a * scale)))
        self.screen.blit(overlay, sq_rect(self.arrow_to, self.flipped))

        # Check highlight on king
        brd = self.board
        if brd.is_check():
            king_sq = brd.king(brd.turn)
            if king_sq is not None:
                r, g, b, a = CHECK_HIGHLIGHT
                overlay.fill((r, g, b, int(a * scale)))
                self.screen.blit(overlay, sq_rect(king_sq, self.flipped))

    def _draw_pieces(self) -> None:
        if self.sliding:
            self._draw_pieces_animated()
        else:
            # Static board
            for sq in chess.SQUARES:
                pc = self.board.piece_at(sq)
                if pc:
                    s = self.sprites.get(pc)
                    if s:
                        self.screen.blit(s, sq_rect(sq, self.flipped))

    def _draw_pieces_animated(self) -> None:
        pre = self.boards[self.move_idx]  # board before this move
        skip = {self.slide_from, self.slide_to}
        if self.slide_rook:
            skip.add(self.rook_from)
            skip.add(self.rook_to)
        if self.ep_capture_sq is not None:
            skip.add(self.ep_capture_sq)

        # Draw static pieces from pre-move board
        for sq in chess.SQUARES:
            if sq in skip:
                continue
            pc = pre.piece_at(sq)
            if pc:
                s = self.sprites.get(pc)
                if s:
                    self.screen.blit(s, sq_rect(sq, self.flipped))

        # Interpolation
        elapsed = pygame.time.get_ticks() - self.slide_start
        t = min(1.0, elapsed / SLIDE_MS)
        t = 1 - (1 - t) ** 3  # ease-out cubic

        # Slide main piece
        if self.slide_piece:
            fr = sq_rect(self.slide_from, self.flipped)
            tr = sq_rect(self.slide_to, self.flipped)
            x = fr.x + (tr.x - fr.x) * t
            y = fr.y + (tr.y - fr.y) * t
            s = self.sprites.get(self.slide_piece)
            if s:
                self.screen.blit(s, (x, y))

        # Slide rook for castling
        if self.slide_rook:
            fr = sq_rect(self.rook_from, self.flipped)
            tr = sq_rect(self.rook_to, self.flipped)
            x = fr.x + (tr.x - fr.x) * t
            y = fr.y + (tr.y - fr.y) * t
            s = self.sprites.get(self.slide_rook)
            if s:
                self.screen.blit(s, (x, y))

        if elapsed >= SLIDE_MS:
            self.sliding = False

    def _draw_arrow(self) -> None:
        a = self._arrow_alpha()
        if a <= 0:
            return
        color = ARROW_CAPTURE_COLOR if self.arrow_capture else ARROW_QUIET_COLOR
        draw_arrow(
            self.screen,
            sq_center(self.arrow_from, self.flipped),
            sq_center(self.arrow_to, self.flipped),
            color,
            a,
        )

    def _draw_info(self) -> None:
        bar = pygame.Rect(0, BOARD_PX, WIN_W, INFO_HEIGHT)
        pygame.draw.rect(self.screen, BG_COLOR, bar)

        white = self.headers.get("White", "?")
        black = self.headers.get("Black", "?")
        result = self.headers.get("Result", "*")
        term = self.headers.get("Termination", "")

        move_num = max(0, (self.move_idx + 2) // 2)
        total = (len(self.moves) + 1) // 2
        side = "w" if (self.move_idx < 0 or self.move_idx % 2 == 1) else "b"
        done = self.move_idx >= len(self.moves) - 1

        y = BOARD_PX + 4

        # Row 1: players + result
        line1 = f"{white}  vs  {black}"
        if done and result != "*":
            line1 += f"    {result}"
            if term:
                line1 += f" ({term})"
        self.screen.blit(self.font.render(line1, True, TEXT_COLOR), (8, y))

        # Row 2: move counter + status
        y += 22
        status = "PAUSED" if self.paused else f"{self.delay}ms"
        ginfo = f"Game {self.game_idx + 1}/{len(self.games)}  |  " if len(self.games) > 1 else ""
        line2 = f"{ginfo}Move {move_num}/{total} ({side})  |  {status}"
        self.screen.blit(self.small_font.render(line2, True, MUTED_TEXT), (8, y))

        # Row 3-4: controls
        y += 20
        # Separator line
        pygame.draw.line(self.screen, (60, 60, 60), (8, y), (WIN_W - 8, y))
        y += 6

        key_color = (170, 170, 170)
        desc_color = MUTED_TEXT
        controls = [
            ("Space", "play/pause"),
            ("\u2190\u2192", "step"),
            ("\u2191\u2193", "speed"),
            ("R", "restart"),
            ("F", "flip"),
        ]
        if len(self.games) > 1:
            controls.append(("G", "next game"))
        controls.append(("Q", "quit"))

        x = 8
        for key, desc in controls:
            key_surf = self.small_font.render(key, True, key_color)
            desc_surf = self.small_font.render(f" {desc}", True, desc_color)
            self.screen.blit(key_surf, (x, y))
            self.screen.blit(desc_surf, (x + key_surf.get_width(), y))
            x += key_surf.get_width() + desc_surf.get_width() + 14
            # Wrap to next row if needed
            if x > WIN_W - 100:
                x = 8
                y += 16

        # End-of-game overlay
        if done and result != "*":
            self._draw_result_overlay(result)

    def _draw_result_overlay(self, result: str) -> None:
        overlay = pygame.Surface((WIN_W, BOARD_PX), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 60))
        self.screen.blit(overlay, (0, 0))
        big = pygame.font.SysFont("monospace", 48, bold=True)
        text = big.render(result, True, (255, 255, 255))
        tx = (WIN_W - text.get_width()) // 2
        ty = (BOARD_PX - text.get_height()) // 2
        # Shadow
        shadow = big.render(result, True, (0, 0, 0))
        self.screen.blit(shadow, (tx + 2, ty + 2))
        self.screen.blit(text, (tx, ty))

    # ---- main loop ----

    def run(self) -> None:
        running = True
        while running:
            self.clock.tick(60)
            now = pygame.time.get_ticks()

            for ev in pygame.event.get():
                if ev.type == pygame.QUIT:
                    running = False
                elif ev.type == pygame.KEYDOWN:
                    running = self._handle_key(ev.key, now)

            # Auto-advance
            if (
                not self.paused
                and not self.sliding
                and self.move_idx < len(self.moves) - 1
                and now - self.last_advance >= self.delay
            ):
                self._advance()
                self.last_advance = now

            # Draw
            self.screen.fill(BG_COLOR)
            self._draw_board()
            self._draw_coords()
            self._draw_highlights()
            self._draw_pieces()
            self._draw_arrow()
            self._draw_info()
            pygame.display.flip()

        pygame.quit()

    def _handle_key(self, key: int, now: int) -> bool:
        if key in (pygame.K_q, pygame.K_ESCAPE):
            return False
        if key == pygame.K_SPACE:
            self.paused = not self.paused
            self.last_advance = now
        elif key in (pygame.K_RIGHT, pygame.K_l):
            if not self.sliding:
                self._advance()
                self.last_advance = now
        elif key in (pygame.K_LEFT, pygame.K_h):
            self._retreat()
            self.last_advance = now
        elif key in (pygame.K_UP, pygame.K_k):
            self.delay = max(100, self.delay - 200)
        elif key in (pygame.K_DOWN, pygame.K_j):
            self.delay = min(5000, self.delay + 200)
        elif key == pygame.K_r:
            self.move_idx = -1
            self._clear_anim()
            self.last_advance = now
        elif key == pygame.K_f:
            self.flipped = not self.flipped
        elif key == pygame.K_g and len(self.games) > 1:
            self.game_idx = (self.game_idx + 1) % len(self.games)
            self._init_game()
            self.last_advance = now
        return True


# ===================================================================
# Entry point
# ===================================================================
def find_latest_pgn() -> str:
    if not os.path.isdir(GAMES_DIR):
        sys.exit(f"No games directory at {GAMES_DIR}")
    pgns = [os.path.join(GAMES_DIR, f) for f in os.listdir(GAMES_DIR) if f.endswith(".pgn")]
    if not pgns:
        sys.exit("No PGN files in games/")
    return max(pgns, key=os.path.getmtime)


def main() -> None:
    parser = argparse.ArgumentParser(description="Animate chess games from PGN files")
    parser.add_argument("pgn", nargs="?", help="PGN file (default: most recent in games/)")
    parser.add_argument("-g", "--game", type=int, default=1, help="Game number in file (1-indexed)")
    args = parser.parse_args()

    path = args.pgn or find_latest_pgn()
    if not os.path.isfile(path):
        alt = os.path.join(GAMES_DIR, path)
        if os.path.isfile(alt):
            path = alt
        else:
            sys.exit(f"Not found: {path}")

    print(f"Loading {path}")
    animator = GameAnimator(path, game_index=args.game - 1)
    animator.run()


if __name__ == "__main__":
    main()
