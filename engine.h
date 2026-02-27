#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "movegen.h"

/*
 * Simple evaluation engine.
 *
 * Evaluates positions and ranks moves. Score is in centipawns
 * from the perspective of the side to move (positive = good for
 * the side to move).
 *
 * This is intentionally a "dumb" engine -- material + piece-square
 * tables + basic mobility. No search tree, no alpha-beta. Just
 * one-ply evaluation to rank moves.
 */

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
 * Evaluate the current position.
 * Returns score in centipawns from White's perspective.
 * Positive = White is better, negative = Black is better.
 */
int evaluate(const Position *p);

/*
 * Rank all legal moves for the side to move.
 * Each move is evaluated by applying it to a copy and evaluating
 * the resulting position. Moves are sorted best-first (highest
 * score for the moving side first).
 */
void rank_moves(const Position *p, RankedMoveList *ranked);

#endif /* ENGINE_H */
