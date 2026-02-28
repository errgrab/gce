#!/usr/bin/env python3
"""GCE Web GUI — Flask + flask-sock server."""

import json
import os
import subprocess
import sys
import threading

import chess
from flask import Flask, send_from_directory
from flask_sock import Sock

ENGINE_PATH = os.path.join(os.path.dirname(__file__), "..", "gce")
WEB_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DEPTH = 6


class Engine:
    """Manages the GCE UCI subprocess (synchronous, one per session)."""

    def __init__(self):
        self.proc = None

    def start(self):
        self.proc = subprocess.Popen(
            [ENGINE_PATH, "--uci"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")
        self._send("isready")
        self._wait_for("readyok")

    def stop(self):
        if self.proc and self.proc.returncode is None:
            try:
                self._send("quit")
                self.proc.terminate()
                self.proc.wait(timeout=2)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()

    def _send(self, cmd):
        if self.proc and self.proc.stdin:
            self.proc.stdin.write(cmd + "\n")
            self.proc.stdin.flush()

    def _readline(self):
        if self.proc and self.proc.stdout:
            return self.proc.stdout.readline().strip()
        return ""

    def _wait_for(self, token):
        while True:
            line = self._readline()
            if not line or line.startswith(token):
                return line
        return ""

    def new_game(self):
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def search(self, fen, depth, info_callback=None):
        """Send position + go, return bestmove UCI string."""
        self._send(f"position fen {fen}")
        self._send(f"go depth {depth}")
        bestmove = None
        while True:
            line = self._readline()
            if not line:
                break
            if line.startswith("info") and info_callback:
                info = self._parse_info(line)
                if info:
                    info_callback(info)
            elif line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    bestmove = parts[1]
                break
        return bestmove

    @staticmethod
    def _parse_info(line):
        tokens = line.split()
        info = {}
        i = 1
        while i < len(tokens):
            key = tokens[i]
            if key == "depth" and i + 1 < len(tokens):
                info["depth"] = int(tokens[i + 1]); i += 2
            elif key == "score" and i + 2 < len(tokens):
                if tokens[i + 1] == "cp":
                    info["score_cp"] = int(tokens[i + 2]); i += 3
                elif tokens[i + 1] == "mate":
                    info["score_mate"] = int(tokens[i + 2]); i += 3
                else:
                    i += 1
            elif key == "nodes" and i + 1 < len(tokens):
                info["nodes"] = int(tokens[i + 1]); i += 2
            elif key == "nps" and i + 1 < len(tokens):
                info["nps"] = int(tokens[i + 1]); i += 2
            elif key == "time" and i + 1 < len(tokens):
                info["time"] = int(tokens[i + 1]); i += 2
            elif key == "pv":
                info["pv"] = " ".join(tokens[i + 1:]); break
            else:
                i += 1
        return info if info else None


class Session:
    """Per-client game session."""

    def __init__(self):
        self.board = chess.Board()
        self.engine = Engine()
        self.depth = DEFAULT_DEPTH
        self.move_history = []  # list of {uci, san, color, number, is_engine, is_capture, is_check, is_castle}

    def start(self):
        self.engine.start()

    def stop(self):
        self.engine.stop()

    def _record_move(self, move, is_engine=False):
        """Record a move in history (call BEFORE pushing to board)."""
        san = self.board.san(move)
        gives_check = self.board.gives_check(move)
        is_capture = self.board.is_capture(move)
        is_castle = self.board.is_castling(move)
        color = "white" if self.board.turn == chess.WHITE else "black"
        rec = {
            "uci": move.uci(),
            "san": san,
            "color": color,
            "number": self.board.fullmove_number,
            "is_engine": is_engine,
            "is_capture": is_capture,
            "is_check": gives_check,
            "is_castle": is_castle,
        }
        self.move_history.append(rec)
        return rec

    def _captured_pieces(self):
        """Compute captured pieces from initial position."""
        initial = {"P": 8, "N": 2, "B": 2, "R": 2, "Q": 1, "K": 1,
                   "p": 8, "n": 2, "b": 2, "r": 2, "q": 1, "k": 1}
        current = {}
        for sq in chess.SQUARES:
            piece = self.board.piece_at(sq)
            if piece:
                s = piece.symbol()
                current[s] = current.get(s, 0) + 1
        # promoted pieces can exceed initial count, clamp
        w_captured = []  # pieces white has captured (black pieces)
        b_captured = []  # pieces black has captured (white pieces)
        for sym in "qrbnp":
            diff = initial.get(sym, 0) - current.get(sym, 0)
            if diff > 0:
                w_captured.extend([sym] * diff)
        for sym in "QRBNP":
            diff = initial.get(sym, 0) - current.get(sym, 0)
            if diff > 0:
                b_captured.extend([sym] * diff)
        return {"white": w_captured, "black": b_captured}

    def get_state(self):
        legal = [m.uci() for m in self.board.legal_moves]
        state = {
            "type": "state",
            "fen": self.board.fen(),
            "legal_moves": legal,
            "turn": "white" if self.board.turn == chess.WHITE else "black",
            "game_over": self.board.is_game_over(),
            "in_check": self.board.is_check(),
            "fullmove": self.board.fullmove_number,
            "halfmove_clock": self.board.halfmove_clock,
            "last_move": self.board.move_stack[-1].uci() if self.board.move_stack else None,
            "move_history": self.move_history,
            "captured": self._captured_pieces(),
        }
        if self.board.is_game_over():
            outcome = self.board.outcome()
            if outcome:
                if outcome.winner is None:
                    state["result"] = "draw"
                elif outcome.winner == chess.WHITE:
                    state["result"] = "white_wins"
                else:
                    state["result"] = "black_wins"
                state["termination"] = outcome.termination.name.lower()
        return state

    def handle(self, raw, send_fn):
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            send_fn(json.dumps({"type": "error", "message": "Invalid JSON"}))
            return

        cmd = msg.get("cmd")

        if cmd == "new_game":
            self.board = chess.Board()
            self.engine.new_game()
            self.move_history = []
            send_fn(json.dumps(self.get_state()))

        elif cmd == "move":
            uci_str = msg.get("move", "")
            try:
                move = chess.Move.from_uci(uci_str)
                if move in self.board.legal_moves:
                    rec = self._record_move(move, is_engine=False)
                    self.board.push(move)
                    state = self.get_state()
                    state["move_info"] = rec
                    send_fn(json.dumps(state))
                else:
                    send_fn(json.dumps({"type": "error", "message": f"Illegal move: {uci_str}"}))
            except (ValueError, chess.InvalidMoveError):
                send_fn(json.dumps({"type": "error", "message": f"Invalid move: {uci_str}"}))

        elif cmd == "go":
            if self.board.is_game_over():
                send_fn(json.dumps({"type": "error", "message": "Game is over"}))
                return
            depth = msg.get("depth", self.depth)

            def info_cb(info):
                send_fn(json.dumps({"type": "thinking", **info}))

            bestmove = self.engine.search(self.board.fen(), depth, info_callback=info_cb)
            if bestmove and bestmove != "(none)":
                try:
                    move = chess.Move.from_uci(bestmove)
                    if move in self.board.legal_moves:
                        rec = self._record_move(move, is_engine=True)
                        self.board.push(move)
                        state = self.get_state()
                        state["engine_move"] = bestmove
                        state["move_info"] = rec
                        send_fn(json.dumps(state))
                    else:
                        send_fn(json.dumps({"type": "error", "message": f"Engine illegal move: {bestmove}"}))
                except (ValueError, chess.InvalidMoveError):
                    send_fn(json.dumps({"type": "error", "message": f"Engine invalid move: {bestmove}"}))
            else:
                send_fn(json.dumps({"type": "error", "message": "Engine returned no move"}))

        elif cmd == "top":
            if self.board.is_game_over():
                send_fn(json.dumps({"type": "error", "message": "Game is over"}))
                return
            depth = msg.get("depth", self.depth)
            results = []
            for move in self.board.legal_moves:
                copy = self.board.copy()
                san = copy.san(move)
                copy.push(move)
                last_score = [None]

                def score_cb(info, ls=last_score):
                    if "score_cp" in info:
                        ls[0] = info["score_cp"]
                    elif "score_mate" in info:
                        ls[0] = info["score_mate"] * 100000

                self.engine.search(copy.fen(), depth - 1, info_callback=score_cb)
                score = -last_score[0] if last_score[0] is not None else 0
                results.append({"move": move.uci(), "san": san, "score_cp": score})

            results.sort(key=lambda r: r["score_cp"], reverse=True)
            send_fn(json.dumps({"type": "top_moves", "moves": results[:5]}))

        elif cmd == "undo":
            if self.board.move_stack:
                self.board.pop()
                if self.move_history:
                    self.move_history.pop()
                send_fn(json.dumps(self.get_state()))
            else:
                send_fn(json.dumps({"type": "error", "message": "Nothing to undo"}))

        elif cmd == "set_depth":
            d = msg.get("depth", DEFAULT_DEPTH)
            if isinstance(d, int) and 1 <= d <= 20:
                self.depth = d
                send_fn(json.dumps({"type": "depth_set", "depth": self.depth}))
            else:
                send_fn(json.dumps({"type": "error", "message": "Depth must be 1-20"}))

        elif cmd == "set_fen":
            fen = msg.get("fen", "")
            try:
                self.board = chess.Board(fen)
                self.engine.new_game()
                self.move_history = []
                send_fn(json.dumps(self.get_state()))
            except ValueError:
                send_fn(json.dumps({"type": "error", "message": f"Invalid FEN: {fen}"}))

        elif cmd == "get_state":
            send_fn(json.dumps(self.get_state()))

        else:
            send_fn(json.dumps({"type": "error", "message": f"Unknown command: {cmd}"}))


# ---------------------------------------------------------------------------
# Flask app
# ---------------------------------------------------------------------------

app = Flask(__name__, static_folder=WEB_DIR)
sock = Sock(app)


@app.route("/")
def index():
    return send_from_directory(WEB_DIR, "index.html")


@app.route("/<path:path>")
def static_files(path):
    return send_from_directory(WEB_DIR, path)


@sock.route("/ws")
def websocket(ws):
    session = Session()
    try:
        session.start()
        ws.send(json.dumps(session.get_state()))
        while True:
            data = ws.receive()
            if data is None:
                break
            session.handle(data, ws.send)
    except Exception as e:
        print(f"[WS] Error: {e}", file=sys.stderr)
    finally:
        session.stop()


if __name__ == "__main__":
    if not os.path.isfile(ENGINE_PATH):
        print(f"[ERROR] Engine not found at {ENGINE_PATH}", file=sys.stderr)
        print("        Run 'make' first to build the engine.", file=sys.stderr)
        sys.exit(1)

    print(f"\n  Open http://localhost:8080 in your browser to play!\n", flush=True)
    app.run(host="0.0.0.0", port=8080, debug=False)
