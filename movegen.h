#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"

/*
 * Move generation module.
 *
 * Generates all legal moves for the current position.
 * A move is "pseudo-legal" if it follows piece movement rules but might
 * leave the king in check. A move is "legal" if it's pseudo-legal AND
 * does not leave the king in check.
 *
 * This module is designed to be reusable by:
 *   - Move validation (is a user move in the legal move list?)
 *   - Engine search (iterate over all legal moves)
 *   - Perft testing (count nodes at depth)
 */

/* Move flags -- packed into a single move for efficiency */
#define MOVE_QUIET       0x00
#define MOVE_DOUBLE_PUSH 0x01  /* Pawn double push (sets en passant) */
#define MOVE_CASTLE_K    0x02  /* Kingside castling */
#define MOVE_CASTLE_Q    0x03  /* Queenside castling */
#define MOVE_CAPTURE     0x04
#define MOVE_EP_CAPTURE  0x05  /* En passant capture */
#define MOVE_PROMO_N     0x08  /* Promotion to knight */
#define MOVE_PROMO_B     0x09  /* Promotion to bishop */
#define MOVE_PROMO_R     0x0A  /* Promotion to rook */
#define MOVE_PROMO_Q     0x0B  /* Promotion to queen */
#define MOVE_PROMO_CAP_N 0x0C  /* Capture + promotion to knight */
#define MOVE_PROMO_CAP_B 0x0D  /* Capture + promotion to bishop */
#define MOVE_PROMO_CAP_R 0x0E  /* Capture + promotion to rook */
#define MOVE_PROMO_CAP_Q 0x0F  /* Capture + promotion to queen */

#define MOVE_IS_PROMO(flags) ((flags) >= MOVE_PROMO_N)
#define MOVE_IS_CAPTURE(flags) ((flags) == MOVE_CAPTURE || \
                                (flags) == MOVE_EP_CAPTURE || \
                                (flags) >= MOVE_PROMO_CAP_N)

/* Full move representation */
typedef struct {
	int from;
	int to;
	int flags;
} Move;

/* Maximum number of legal moves in any chess position (generous bound) */
#define MAX_MOVES 256

/* Move list: array + count */
typedef struct {
	Move moves[MAX_MOVES];
	int count;
} MoveList;

/* ---- Core generation functions ---- */

/* Generate all pseudo-legal moves (may leave king in check) */
void generate_pseudo_legal(const Position *p, MoveList *list);

/* Generate all legal moves (filters out moves that leave king in check) */
void generate_legal_moves(const Position *p, MoveList *list);

/* ---- Query functions ---- */

/* Check if a given from/to move is in the legal move list.
 * If found, copies the full Move (with flags) into *out and returns true.
 * If promo_piece is set (N/B/R/Q), it matches that promotion type. */
bool is_move_legal(const Position *p, int from, int to,
                   PieceType promo_piece, Move *out);

/* Count of legal moves (useful for checkmate/stalemate detection) */
int count_legal_moves(const Position *p);

/* ---- Utility: coordinate notation (UCI style) ---- */

/* Convert move to coordinate string (e.g. "e2e4", "e7e8q") */
void move_to_str(const Move *m, char *buf);

/* Parse coordinate notation string into a Move.
 * Returns true on success. */
bool parse_move(const char *str, const Position *p, Move *m);

/* ---- Utility: Standard Algebraic Notation (SAN) ---- */

/*
 * Convert a move to SAN string (e.g. "e4", "Nf3", "Qxf7+", "O-O", "exd5",
 * "Rae1", "e8=Q"). Buffer should be at least 12 bytes.
 * The position must be the state BEFORE the move is made.
 */
void move_to_san(const Move *m, const Position *p, char *buf);

/*
 * Parse a SAN string (e.g. "e4", "Nf3", "Qh5", "O-O", "exd5", "Rae1",
 * "e8=Q") into a Move. The position is needed to resolve ambiguity.
 * Returns true on success, false if the SAN is invalid or no legal move
 * matches.
 */
bool parse_san(const char *str, const Position *p, Move *m);

#endif /* MOVEGEN_H */
