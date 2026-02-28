#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"

#define MOVE_QUIET       0x00
#define MOVE_DOUBLE_PUSH 0x01
#define MOVE_CASTLE_K    0x02
#define MOVE_CASTLE_Q    0x03
#define MOVE_CAPTURE     0x04
#define MOVE_EP_CAPTURE  0x05
#define MOVE_PROMO_N     0x08
#define MOVE_PROMO_B     0x09
#define MOVE_PROMO_R     0x0A
#define MOVE_PROMO_Q     0x0B
#define MOVE_PROMO_CAP_N 0x0C
#define MOVE_PROMO_CAP_B 0x0D
#define MOVE_PROMO_CAP_R 0x0E
#define MOVE_PROMO_CAP_Q 0x0F

#define PROMO_PIECE_MASK 0x03
#define MOVE_IS_PROMO(f)   ((f) >= MOVE_PROMO_N)
#define MOVE_IS_CAPTURE(f) ((f) == MOVE_CAPTURE || \
                            (f) == MOVE_EP_CAPTURE || \
                            (f) >= MOVE_PROMO_CAP_N)

static inline PieceType promo_type_from_flags(int flags) {
	static const PieceType t[4] = { KNIGHT, BISHOP, ROOK, QUEEN };
	return t[flags & PROMO_PIECE_MASK];
}

typedef struct { int from, to, flags; } Move;

#define MAX_MOVES 256

typedef struct {
	Move moves[MAX_MOVES];
	int count;
} MoveList;

void generate_pseudo_legal(const Position *p, MoveList *list);
void generate_legal_moves(const Position *p, MoveList *list);
void generate_legal_captures(const Position *p, MoveList *list);
bool is_move_legal(const Position *p, int from, int to,
                   PieceType promo_piece, Move *out);
int  count_legal_moves(const Position *p);
void move_to_str(const Move *m, char *buf);
bool parse_move(const char *str, const Position *p, Move *m);
void move_to_san(const Move *m, const Position *p, char *buf);
bool parse_san(const char *str, const Position *p, Move *m);

#endif
