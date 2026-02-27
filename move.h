#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include "movegen.h"

/*
 * High-level move API.
 *
 * This module ties together parsing, validation, and execution.
 * It's the public interface that main.c and future frontends use.
 */

/* Game state flags */
typedef enum {
	GAME_ONGOING,
	GAME_CHECKMATE,
	GAME_STALEMATE,
	GAME_DRAW_50,        /* 50-move rule */
	GAME_DRAW_MATERIAL   /* Insufficient material */
} GameState;

/*
 * Try to make a move from user input.
 * Accepts SAN (e.g. "e4", "Nf3", "O-O") or coordinate notation ("e2e4").
 * Returns NULL on success, or an error message on failure.
 * On success, the position is updated in-place.
 * If out_move is non-NULL and the move succeeds, the parsed Move is stored
 * there (useful for display after execution).
 */
const char *try_make_move(Position *p, const char *move_str, Move *out_move);

/*
 * Make a move given a validated Move struct.
 * This applies the move unconditionally (caller must ensure legality).
 */
void make_move(Position *p, const Move *m);

/*
 * Determine the current game state.
 */
GameState get_game_state(const Position *p);

/*
 * Return a human-readable string for a GameState.
 */
const char *game_state_str(GameState state);

#endif /* MOVE_H */
