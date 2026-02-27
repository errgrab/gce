#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "movegen.h"

/*
 * Chess engine with alpha-beta search and pruning optimizations.
 *
 * Features:
 *   - Negamax with alpha-beta pruning
 *   - Transposition table (Zobrist hashing)
 *   - Iterative deepening with best-move ordering
 *   - Quiescence search (captures only at horizon)
 *   - Null move pruning
 *   - Killer move heuristic
 *   - Late move reductions (LMR)
 *   - Check extensions
 */

/* Default search depth (half-moves) */
#define DEFAULT_DEPTH 6

/* Score constants */
#define SCORE_INF    1000000
#define SCORE_MATE   999000

/* Maximum search depth for internal arrays (killers, etc.) */
#define MAX_PLY 128

/* A scored move: a legal move plus its evaluation */
typedef struct {
	Move move;
	int score;   /* centipawns, positive = good for the moving side */
} ScoredMove;

/* Ranked move list */
typedef struct {
	ScoredMove moves[MAX_MOVES];
	int count;
} RankedMoveList;

/*
 * Initialize the engine (clear TT, killers, etc.).
 * Call once at startup or when starting a new game.
 */
void engine_init(void);

/*
 * Evaluate the current position (static evaluation).
 * Returns score in centipawns from White's perspective.
 */
int evaluate(const Position *p);

/*
 * Search for the best move using iterative deepening + alpha-beta.
 * Returns the evaluation score from the side-to-move's perspective.
 * If best_move is non-NULL, the best move found is stored there.
 */
int engine_search(const Position *p, int max_depth, Move *best_move);

/*
 * Rank all legal moves for the side to move.
 * Uses iterative deepening search at DEFAULT_DEPTH.
 * Moves are sorted best-first.
 */
void rank_moves(const Position *p, RankedMoveList *ranked);

#endif /* ENGINE_H */
