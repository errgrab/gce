#ifndef ATTACK_H
#define ATTACK_H

#include "board.h"

/*
 * Attack generation module.
 *
 * Each function returns a Bitboard of squares attacked by a piece
 * at the given square. For sliding pieces (bishop, rook, queen),
 * an occupancy bitboard is needed to determine blocking.
 *
 * These functions are the building blocks for:
 *   - Move generation (movegen.c)
 *   - Move validation (is this move legal?)
 *   - Square attack detection (is square attacked?)
 *   - Future engine evaluation (mobility, king safety, etc.)
 */

/* ---- Non-sliding pieces (lookup table based) ---- */

/* Squares attacked by a pawn of the given color on sq.
 * NOTE: This is ATTACK squares only, not push squares. */
Bitboard pawn_attacks(int sq, Color side);

/* Squares attacked by a knight on sq */
Bitboard knight_attacks(int sq);

/* Squares attacked by a king on sq */
Bitboard king_attacks(int sq);

/* ---- Sliding pieces (classical approach with ray scanning) ---- */

/* Squares attacked by a bishop on sq given occupancy */
Bitboard bishop_attacks(int sq, Bitboard occ);

/* Squares attacked by a rook on sq given occupancy */
Bitboard rook_attacks(int sq, Bitboard occ);

/* Squares attacked by a queen on sq given occupancy (bishop | rook) */
Bitboard queen_attacks(int sq, Bitboard occ);

/* ---- Initialization ---- */

/* Pre-compute attack tables. Call once at program start. */
void init_attacks(void);

#endif /* ATTACK_H */
