#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include "movegen.h"

typedef enum {
	GAME_ONGOING,
	GAME_CHECKMATE,
	GAME_STALEMATE,
	GAME_DRAW_50,
	GAME_DRAW_MATERIAL
} GameState;

const char *try_make_move(Position *p, const char *move_str, Move *out_move);
void        make_move(Position *p, const Move *m);
GameState   get_game_state(const Position *p);
const char *game_state_str(GameState state);

#endif
