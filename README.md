# GCE - G Chess Engine

A chess engine written in C99 with UCI protocol support, a web-based GUI, and a live presence on Lichess.

## Play Against It

- **Lichess:** Challenge [TheGBot](https://lichess.org/@/TheGBot) -- note that it may sometimes be offline as it runs on a local machine.
- **Web UI:** Run the included web server for a full browser-based interface with drag-and-drop, move animations, sound effects, and real-time engine analysis.

## Features

### Engine

- Bitboard-based board representation with incremental Zobrist hashing
- Negamax search with alpha-beta pruning
- Iterative deepening with aspiration windows
- Principal Variation Search (PVS)
- Late Move Reductions (LMR)
- Null move pruning
- Quiescence search with delta pruning
- Transposition table (1M entries)
- Move ordering: TT move, MVV-LVA, killer heuristic, history heuristic
- Check extensions
- Piece-square tables, bishop pair bonus, pawn structure evaluation (doubled/isolated/passed pawns), king safety (pawn shield), and mobility scoring
- Time management with support for fixed depth, fixed movetime, and clock-based allocation

### UCI Protocol

Implements the [Universal Chess Interface](https://en.wikipedia.org/wiki/Universal_Chess_Interface) protocol. Supports `position`, `go` (with `depth`, `movetime`, `wtime`/`btime`/`winc`/`binc`, `movestogo`, `infinite`), `stop`, `ucinewgame`, and more. Compatible with any UCI-compliant GUI (Arena, CuteChess, etc.).

### Web Interface

A self-contained browser UI served by a Python/Flask backend that communicates with the engine over WebSocket:

- Drag-and-drop piece movement (mouse and touch)
- Move animations and programmatic sound effects (Web Audio API, no audio files)
- Real-time thinking info display (depth, nodes, kn/s, PV line)
- Top 5 move analysis
- Evaluation bar, move history, captured pieces
- Auto-reply mode, board flipping, undo
- Keyboard shortcuts: Space/Enter (engine), Z (undo), F (flip), N (new game), T (top moves)

## Building

Requires GCC and a POSIX environment (Linux/macOS).

```sh
make
```

## Usage

### Interactive CLI

```sh
./gce
```

Play by entering moves in SAN notation (e.g. `e4`, `Nf3`, `O-O`). Type `eval` to see the current evaluation or `top` to see the top moves.

### UCI Mode

```sh
./gce --uci
```

Or type `uci` at the interactive prompt. This is the mode used by chess GUIs and the web interface.

### Web Interface

```sh
cd web
pip install -r requirements.txt
python server.py
```

Then open `http://localhost:8080` in your browser.

## Project Structure

```
├── main.c          # Entry point, interactive CLI
├── board.c/h       # Position representation, FEN parsing, Zobrist hashing
├── attack.c/h      # Attack tables, sliding piece ray generation
├── movegen.c/h     # Move generation, SAN/coordinate parsing
├── move.c/h        # Make-move logic, game state detection
├── engine.c/h      # Search, evaluation, transposition table
├── uci.c/h         # UCI protocol implementation
├── Makefile        # Build configuration
└── web/
    ├── server.py       # Flask + WebSocket backend
    ├── index.html      # Single-page frontend (~1150 lines)
    ├── requirements.txt
    └── img/            # SVG piece assets
```

## License

[GPLv3](LICENSE)
