#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t Bitboard;

#define CASTLE_WK  0x01
#define CASTLE_WQ  0x02
#define CASTLE_BK  0x04
#define CASTLE_BQ  0x08
#define CASTLE_ALL (CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ)

#define SQ_A1 0
#define SQ_B1 1
#define SQ_C1 2
#define SQ_D1 3
#define SQ_E1 4
#define SQ_F1 5
#define SQ_G1 6
#define SQ_H1 7
#define SQ_A8 56
#define SQ_B8 57
#define SQ_C8 58
#define SQ_D8 59
#define SQ_E8 60
#define SQ_F8 61
#define SQ_G8 62
#define SQ_H8 63

#define SQ_FILE(sq) ((sq) & 7)
#define SQ_RANK(sq) ((sq) >> 3)

typedef enum {
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_NONE
} PieceType;

#define NUM_PIECE_TYPES 6

typedef enum { WHITE = 0, BLACK = 1 } Color;

typedef struct {
	Bitboard pieces[2][NUM_PIECE_TYPES];
	bool white_turn;
	uint8_t castling;
	int en_passant;
	int halfmove;
	int fullmove;
	uint64_t hash;
} Position;

void     init_zobrist(void);
uint64_t compute_hash(const Position *p);
uint64_t zobrist_piece_key(int color, int piece_type, int sq);
uint64_t zobrist_side_key(void);
uint64_t zobrist_castling_key(int rights);
uint64_t zobrist_ep_key(int file);

void init_position(Position *p);
bool position_from_fen(Position *p, const char *fen);

Bitboard  occupied(const Position *p);
Bitboard  pieces_by_color(const Position *p, Color c);
char      piece_at(const Position *p, int sq);
PieceType piece_type_at(const Position *p, int sq);
Color     piece_color_at(const Position *p, int sq);
void      print_board(const Position *p);

bool is_square_attacked(const Position *p, int sq, Color by);
bool is_in_check(const Position *p);

#endif
