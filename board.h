#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t Bitboard;

/* Castling rights bitmask */
#define CASTLE_WK 0x01  /* White kingside  (O-O)  */
#define CASTLE_WQ 0x02  /* White queenside (O-O-O) */
#define CASTLE_BK 0x04  /* Black kingside  (O-O)  */
#define CASTLE_BQ 0x08  /* Black queenside (O-O-O) */
#define CASTLE_ALL (CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ)

/* Piece type enum -- used for promotion and identification */
typedef enum {
	PIECE_NONE = 0,
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
} PieceType;

/* Color enum */
typedef enum {
	WHITE = 0, BLACK = 1
} Color;

typedef struct {
	/* White pieces */
	Bitboard wp, wn, wb, wr, wq, wk;
	/* Black pieces */
	Bitboard bp, bn, bb, br, bq, bk;

	bool white_turn;
	uint8_t castling;      /* bitmask of CASTLE_* flags */
	int en_passant;        /* target square index (0-63) or -1 if none */
	int halfmove;          /* halfmove clock for 50-move rule */
	int fullmove;          /* fullmove counter (starts at 1) */
	uint64_t hash;         /* Zobrist hash of the position */
} Position;

/* ---- Zobrist hashing ---- */
void init_zobrist(void);
uint64_t compute_hash(const Position *p);

/* ---- Initialization ---- */
void init_position(Position *p);

/* ---- Bitboard queries ---- */
Bitboard white_pieces(const Position *p);
Bitboard black_pieces(const Position *p);
Bitboard occupied(const Position *p);
Bitboard pieces_by_color(const Position *p, Color c);

/* ---- Square queries ---- */
char piece_at(const Position *p, int sq);
PieceType piece_type_at(const Position *p, int sq);
Color piece_color_at(const Position *p, int sq);  /* undefined if empty */

/* Return a pointer to the bitboard that contains the piece at sq, or NULL */
Bitboard *find_piece_bb(Position *p, int sq);

/* ---- Display ---- */
void print_board(const Position *p);

/* ---- Square coordinate helpers ---- */
int square_from_coords(int file, int rank);
int square_file(int sq);
int square_rank(int sq);

/* ---- Utility ---- */
/* Copy position (for make/unmake or lookahead) */
void copy_position(Position *dst, const Position *src);

/* Check if a square is attacked by a given color */
bool is_square_attacked(const Position *p, int sq, Color by);

/* Check if the side to move is in check */
bool is_in_check(const Position *p);

#endif /* BOARD_H */
