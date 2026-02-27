#include "board.h"
#include "attack.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Zobrist hashing
 *
 * Random 64-bit keys for each (piece, square) combination, plus
 * side-to-move, castling rights, and en passant file.
 * ================================================================ */

/* piece_keys[color][piece_type][square] -- color 0=W, 1=B; piece 0-5 = P,N,B,R,Q,K */
static uint64_t piece_keys[2][6][64];
static uint64_t side_key;                /* XOR when Black to move */
static uint64_t castling_keys[16];       /* one key per castling bitmask */
static uint64_t ep_keys[8];             /* one key per en passant file */

/* Simple xorshift64 PRNG for deterministic key generation */
static uint64_t xorshift64(uint64_t *state) {
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	*state = x;
	return x;
}

void init_zobrist(void) {
	uint64_t state = 0x4D595A6F62726973ULL; /* seed: "MYZobris" */

	for (int c = 0; c < 2; c++)
		for (int p = 0; p < 6; p++)
			for (int s = 0; s < 64; s++)
				piece_keys[c][p][s] = xorshift64(&state);

	side_key = xorshift64(&state);

	for (int i = 0; i < 16; i++)
		castling_keys[i] = xorshift64(&state);

	for (int i = 0; i < 8; i++)
		ep_keys[i] = xorshift64(&state);
}

/* Compute hash from scratch (for init or verification) */
uint64_t compute_hash(const Position *p) {
	uint64_t h = 0;

	/* Map each bitboard to (color, piece_index) */
	const Bitboard *bbs[12] = {
		&p->wp, &p->wn, &p->wb, &p->wr, &p->wq, &p->wk,
		&p->bp, &p->bn, &p->bb, &p->br, &p->bq, &p->bk
	};

	for (int i = 0; i < 12; i++) {
		int color = i / 6;       /* 0 = white, 1 = black */
		int piece = i % 6;       /* 0=P, 1=N, 2=B, 3=R, 4=Q, 5=K */
		Bitboard bb = *bbs[i];
		while (bb) {
			int sq = __builtin_ctzll(bb);
			h ^= piece_keys[color][piece][sq];
			bb &= bb - 1;
		}
	}

	if (!p->white_turn) h ^= side_key;
	h ^= castling_keys[p->castling & 0x0F];
	if (p->en_passant >= 0) h ^= ep_keys[p->en_passant & 7];

	return h;
}

void init_position(Position *p) {
	memset(p, 0, sizeof(Position));

	/* White pieces (ranks 1-2) */
	p->wp = 0x000000000000FF00ULL;
	p->wr = 0x0000000000000081ULL;
	p->wn = 0x0000000000000042ULL;
	p->wb = 0x0000000000000024ULL;
	p->wq = 0x0000000000000008ULL;
	p->wk = 0x0000000000000010ULL;

	/* Black pieces (ranks 7-8) */
	p->bp = 0x00FF000000000000ULL;
	p->br = 0x8100000000000000ULL;
	p->bn = 0x4200000000000000ULL;
	p->bb = 0x2400000000000000ULL;
	p->bq = 0x0800000000000000ULL;
	p->bk = 0x1000000000000000ULL;

	p->white_turn = true;
	p->castling   = CASTLE_ALL;
	p->en_passant = -1;
	p->halfmove   = 0;
	p->fullmove   = 1;
	p->hash       = compute_hash(p);
}

Bitboard white_pieces(const Position *p) {
	return p->wp | p->wn | p->wb | p->wr | p->wq | p->wk;
}

Bitboard black_pieces(const Position *p) {
	return p->bp | p->bn | p->bb | p->br | p->bq | p->bk;
}

Bitboard occupied(const Position *p) {
	return white_pieces(p) | black_pieces(p);
}

Bitboard pieces_by_color(const Position *p, Color c) {
	return c == WHITE ? white_pieces(p) : black_pieces(p);
}

char piece_at(const Position *p, int sq) {
	Bitboard mask = 1ULL << sq;

	if (p->wp & mask) return 'P';
	if (p->wn & mask) return 'N';
	if (p->wb & mask) return 'B';
	if (p->wr & mask) return 'R';
	if (p->wq & mask) return 'Q';
	if (p->wk & mask) return 'K';
	if (p->bp & mask) return 'p';
	if (p->bn & mask) return 'n';
	if (p->bb & mask) return 'b';
	if (p->br & mask) return 'r';
	if (p->bq & mask) return 'q';
	if (p->bk & mask) return 'k';

	return '.';
}

PieceType piece_type_at(const Position *p, int sq) {
	Bitboard mask = 1ULL << sq;

	if ((p->wp | p->bp) & mask) return PAWN;
	if ((p->wn | p->bn) & mask) return KNIGHT;
	if ((p->wb | p->bb) & mask) return BISHOP;
	if ((p->wr | p->br) & mask) return ROOK;
	if ((p->wq | p->bq) & mask) return QUEEN;
	if ((p->wk | p->bk) & mask) return KING;

	return PIECE_NONE;
}

Color piece_color_at(const Position *p, int sq) {
	Bitboard mask = 1ULL << sq;
	if (white_pieces(p) & mask) return WHITE;
	return BLACK;
}

Bitboard *find_piece_bb(Position *p, int sq) {
	Bitboard mask = 1ULL << sq;

	if (p->wp & mask) return &p->wp;
	if (p->wn & mask) return &p->wn;
	if (p->wb & mask) return &p->wb;
	if (p->wr & mask) return &p->wr;
	if (p->wq & mask) return &p->wq;
	if (p->wk & mask) return &p->wk;
	if (p->bp & mask) return &p->bp;
	if (p->bn & mask) return &p->bn;
	if (p->bb & mask) return &p->bb;
	if (p->br & mask) return &p->br;
	if (p->bq & mask) return &p->bq;
	if (p->bk & mask) return &p->bk;

	return NULL;
}

void print_board(const Position *p) {
	printf("  a b c d e f g h\n");
	for (int rank = 7; rank >= 0; rank--) {
		printf("%d ", rank + 1);
		for (int file = 0; file < 8; file++) {
			int sq = rank * 8 + file;
			printf("%c ", piece_at(p, sq));
		}
		printf("%d\n", rank + 1);
	}
	printf("  a b c d e f g h\n");

	printf("\n%s to move", p->white_turn ? "White" : "Black");
	printf("  Castling: %c%c%c%c",
		(p->castling & CASTLE_WK) ? 'K' : '-',
		(p->castling & CASTLE_WQ) ? 'Q' : '-',
		(p->castling & CASTLE_BK) ? 'k' : '-',
		(p->castling & CASTLE_BQ) ? 'q' : '-');
	if (p->en_passant >= 0)
		printf("  EP: %c%d",
			'a' + square_file(p->en_passant),
			1 + square_rank(p->en_passant));
	printf("\n\n");
}

int square_from_coords(int file, int rank) {
	return rank * 8 + file;
}

int square_file(int sq) {
	return sq & 7;
}

int square_rank(int sq) {
	return sq >> 3;
}

void copy_position(Position *dst, const Position *src) {
	*dst = *src;
}

bool is_square_attacked(const Position *p, int sq, Color by) {
	Bitboard occ = occupied(p);

	/* Knight attacks */
	Bitboard knights = (by == WHITE) ? p->wn : p->bn;
	if (knight_attacks(sq) & knights) return true;

	/* King attacks */
	Bitboard kings = (by == WHITE) ? p->wk : p->bk;
	if (king_attacks(sq) & kings) return true;

	/* Pawn attacks */
	Bitboard pawns = (by == WHITE) ? p->wp : p->bp;
	/* If attacked BY white, we check if a white pawn on a nearby square
	   could attack sq -- that means sq is attacked from below-left/below-right */
	if (by == WHITE) {
		if (pawn_attacks(sq, BLACK) & pawns) return true;
	} else {
		if (pawn_attacks(sq, WHITE) & pawns) return true;
	}

	/* Bishop / Queen (diagonal) attacks */
	Bitboard diag = (by == WHITE) ? (p->wb | p->wq) : (p->bb | p->bq);
	if (bishop_attacks(sq, occ) & diag) return true;

	/* Rook / Queen (straight) attacks */
	Bitboard straight = (by == WHITE) ? (p->wr | p->wq) : (p->br | p->bq);
	if (rook_attacks(sq, occ) & straight) return true;

	return false;
}

bool is_in_check(const Position *p) {
	Color side = p->white_turn ? WHITE : BLACK;
	Color enemy = p->white_turn ? BLACK : WHITE;
	Bitboard king = (side == WHITE) ? p->wk : p->bk;

	/* Find the king square */
	int king_sq = __builtin_ctzll(king); /* index of lowest set bit */

	return is_square_attacked(p, king_sq, enemy);
}
