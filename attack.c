#include "attack.h"

/* ================================================================
 * Pre-computed attack tables for non-sliding pieces.
 * Indexed by square (0-63).
 * ================================================================ */

static Bitboard pawn_attack_table[2][64];  /* [color][square] */
static Bitboard knight_attack_table[64];
static Bitboard king_attack_table[64];

/* ---- File masks used to prevent wrapping ---- */
#define FILE_A 0x0101010101010101ULL
#define FILE_H 0x8080808080808080ULL
#define FILE_AB (FILE_A | (FILE_A << 1))
#define FILE_GH (FILE_H | (FILE_H >> 1))

/* ================================================================
 * Initialization: compute all tables at startup
 * ================================================================ */

static void init_pawn_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq;
		/* White pawns attack up-left and up-right */
		pawn_attack_table[WHITE][sq] =
			((bb & ~FILE_A) << 7) | ((bb & ~FILE_H) << 9);
		/* Black pawns attack down-left and down-right */
		pawn_attack_table[BLACK][sq] =
			((bb & ~FILE_H) >> 7) | ((bb & ~FILE_A) >> 9);
	}
}

static void init_knight_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq;
		Bitboard atk = 0;

		/* All 8 knight jumps, masked to prevent file wrapping */
		atk |= (bb & ~FILE_A)  << 15;  /* up 2, left 1  */
		atk |= (bb & ~FILE_H)  << 17;  /* up 2, right 1 */
		atk |= (bb & ~FILE_AB) << 6;   /* up 1, left 2  */
		atk |= (bb & ~FILE_GH) << 10;  /* up 1, right 2 */
		atk |= (bb & ~FILE_H)  >> 15;  /* down 2, right 1 */
		atk |= (bb & ~FILE_A)  >> 17;  /* down 2, left 1  */
		atk |= (bb & ~FILE_GH) >> 6;   /* down 1, right 2 */
		atk |= (bb & ~FILE_AB) >> 10;  /* down 1, left 2  */

		knight_attack_table[sq] = atk;
	}
}

static void init_king_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq;
		Bitboard atk = 0;

		atk |= (bb & ~FILE_A) << 7;   /* up-left    */
		atk |= bb << 8;               /* up         */
		atk |= (bb & ~FILE_H) << 9;   /* up-right   */
		atk |= (bb & ~FILE_H) << 1;   /* right      */
		atk |= (bb & ~FILE_H) >> 7;   /* down-right */
		atk |= bb >> 8;               /* down       */
		atk |= (bb & ~FILE_A) >> 9;   /* down-left  */
		atk |= (bb & ~FILE_A) >> 1;   /* left       */

		king_attack_table[sq] = atk;
	}
}

void init_attacks(void) {
	init_pawn_attacks();
	init_knight_attacks();
	init_king_attacks();
}

/* ================================================================
 * Public lookup functions
 * ================================================================ */

Bitboard pawn_attacks(int sq, Color side) {
	return pawn_attack_table[side][sq];
}

Bitboard knight_attacks(int sq) {
	return knight_attack_table[sq];
}

Bitboard king_attacks(int sq) {
	return king_attack_table[sq];
}

/* ================================================================
 * Sliding piece attacks -- classical ray approach.
 *
 * We scan in each direction until we hit the board edge or a
 * blocking piece. The blocking piece's square IS included in the
 * attack set (it can be captured).
 *
 * This is simple and correct. For a future engine, this can be
 * replaced with magic bitboards for speed without changing the
 * interface.
 * ================================================================ */

/* Direction offsets: { file_delta, rank_delta } */
static const int bishop_dirs[4][2] = {
	{ -1,  1 }, {  1,  1 },   /* up-left, up-right   */
	{ -1, -1 }, {  1, -1 }    /* down-left, down-right */
};

static const int rook_dirs[4][2] = {
	{  0,  1 }, {  0, -1 },   /* up, down */
	{ -1,  0 }, {  1,  0 }    /* left, right */
};

static Bitboard slide_attacks(int sq, Bitboard occ,
                              const int dirs[4][2], int ndirs)
{
	Bitboard atk = 0;
	int f0 = sq & 7;
	int r0 = sq >> 3;

	for (int d = 0; d < ndirs; d++) {
		int df = dirs[d][0];
		int dr = dirs[d][1];
		int f = f0 + df;
		int r = r0 + dr;

		while (f >= 0 && f <= 7 && r >= 0 && r <= 7) {
			int target = r * 8 + f;
			Bitboard target_bb = 1ULL << target;
			atk |= target_bb;

			/* Stop after hitting a piece (but include that square) */
			if (occ & target_bb) break;

			f += df;
			r += dr;
		}
	}

	return atk;
}

Bitboard bishop_attacks(int sq, Bitboard occ) {
	return slide_attacks(sq, occ, bishop_dirs, 4);
}

Bitboard rook_attacks(int sq, Bitboard occ) {
	return slide_attacks(sq, occ, rook_dirs, 4);
}

Bitboard queen_attacks(int sq, Bitboard occ) {
	return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}
