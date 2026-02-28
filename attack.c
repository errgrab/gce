#include "attack.h"

static Bitboard pawn_attack_table[2][64];
static Bitboard knight_attack_table[64];
static Bitboard king_attack_table[64];

#define FILE_A  0x0101010101010101ULL
#define FILE_H  0x8080808080808080ULL
#define FILE_AB (FILE_A | (FILE_A << 1))
#define FILE_GH (FILE_H | (FILE_H >> 1))

static void init_pawn_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq;
		pawn_attack_table[WHITE][sq] =
			((bb & ~FILE_A) << 7) | ((bb & ~FILE_H) << 9);
		pawn_attack_table[BLACK][sq] =
			((bb & ~FILE_H) >> 7) | ((bb & ~FILE_A) >> 9);
	}
}

static void init_knight_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq, a = 0;
		a |= (bb & ~FILE_A)  << 15;  a |= (bb & ~FILE_H)  << 17;
		a |= (bb & ~FILE_AB) << 6;   a |= (bb & ~FILE_GH) << 10;
		a |= (bb & ~FILE_H)  >> 15;  a |= (bb & ~FILE_A)  >> 17;
		a |= (bb & ~FILE_GH) >> 6;   a |= (bb & ~FILE_AB) >> 10;
		knight_attack_table[sq] = a;
	}
}

static void init_king_attacks(void) {
	for (int sq = 0; sq < 64; sq++) {
		Bitboard bb = 1ULL << sq, a = 0;
		a |= (bb & ~FILE_A) << 7;  a |= bb << 8;  a |= (bb & ~FILE_H) << 9;
		a |= (bb & ~FILE_H) << 1;  a |= (bb & ~FILE_H) >> 7;
		a |= bb >> 8;  a |= (bb & ~FILE_A) >> 9;  a |= (bb & ~FILE_A) >> 1;
		king_attack_table[sq] = a;
	}
}

void init_attacks(void) {
	init_pawn_attacks();
	init_knight_attacks();
	init_king_attacks();
}

Bitboard pawn_attacks(int sq, Color side)   { return pawn_attack_table[side][sq]; }
Bitboard knight_attacks(int sq)             { return knight_attack_table[sq]; }
Bitboard king_attacks(int sq)               { return king_attack_table[sq]; }

/* Classical ray scanning for sliding pieces */
static const int bishop_dirs[4][2] = {{-1,1},{1,1},{-1,-1},{1,-1}};
static const int rook_dirs[4][2]   = {{0,1},{0,-1},{-1,0},{1,0}};

static Bitboard slide_attacks(int sq, Bitboard occ,
                              const int dirs[4][2], int ndirs) {
	Bitboard atk = 0;
	int f0 = sq & 7, r0 = sq >> 3;
	for (int d = 0; d < ndirs; d++) {
		int df = dirs[d][0], dr = dirs[d][1];
		int f = f0 + df, r = r0 + dr;
		while (f >= 0 && f <= 7 && r >= 0 && r <= 7) {
			Bitboard tb = 1ULL << (r * 8 + f);
			atk |= tb;
			if (occ & tb) break;
			f += df; r += dr;
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
