#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "movegen.h"

#define DEFAULT_DEPTH 6
#define SCORE_INF     1000000
#define SCORE_MATE    999000
#define MAX_PLY       128

void engine_init(void);
int  evaluate(const Position *p);
int  engine_search(const Position *p, int max_depth, Move *best_move);
int  engine_search_uci(const Position *p, int max_depth,
                       int64_t time_limit_ms, Move *best_move);

extern volatile int engine_stop;

typedef void (*EngineCheckFn)(void);
extern EngineCheckFn engine_check_fn;

#endif
