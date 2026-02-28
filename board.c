#include "board.h"
#include "attack.h"
#include <stdio.h>
#include <string.h>

static uint64_t piece_keys[2][NUM_PIECE_TYPES][64];
static uint64_t side_key;
static uint64_t castling_keys[16];
static uint64_t ep_keys[8];

static uint64_t xorshift64(uint64_t *state) {
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	*state = x;
	return x;
}

void init_zobrist(void) {
	uint64_t state = 0x4D595A6F62726973ULL;
	for (int c = 0; c < 2; c++)
		for (int p = 0; p < NUM_PIECE_TYPES; p++)
			for (int s = 0; s < 64; s++)
				piece_keys[c][p][s] = xorshift64(&state);
	side_key = xorshift64(&state);
	for (int i = 0; i < 16; i++)
		castling_keys[i] = xorshift64(&state);
	for (int i = 0; i < 8; i++)
		ep_keys[i] = xorshift64(&state);
}

uint64_t zobrist_piece_key(int color, int piece_type, int sq) {
	return piece_keys[color][piece_type][sq];
}
uint64_t zobrist_side_key(void)        { return side_key; }
uint64_t zobrist_castling_key(int r)   { return castling_keys[r & 0x0F]; }
uint64_t zobrist_ep_key(int file)      { return ep_keys[file & 7]; }

uint64_t compute_hash(const Position *p) {
	uint64_t h = 0;
	for (int c = 0; c < 2; c++)
		for (int pt = 0; pt < NUM_PIECE_TYPES; pt++) {
			Bitboard bb = p->pieces[c][pt];
			while (bb) {
				int sq = __builtin_ctzll(bb);
				h ^= piece_keys[c][pt][sq];
				bb &= bb - 1;
			}
		}
	if (!p->white_turn) h ^= side_key;
	h ^= castling_keys[p->castling & 0x0F];
	if (p->en_passant >= 0) h ^= ep_keys[p->en_passant & 7];
	return h;
}

bool position_from_fen(Position *p, const char *fen) {
	memset(p, 0, sizeof(Position));
	p->en_passant = -1;
	const char *s = fen;

	for (int rank = 7; rank >= 0; rank--) {
		int file = 0;
		while (file < 8) {
			char c = *s++;
			if (c >= '1' && c <= '8') {
				file += c - '0';
			} else {
				int sq = rank * 8 + file;
				Bitboard bit = 1ULL << sq;
				switch (c) {
				case 'P': p->pieces[WHITE][PAWN]   |= bit; break;
				case 'N': p->pieces[WHITE][KNIGHT] |= bit; break;
				case 'B': p->pieces[WHITE][BISHOP] |= bit; break;
				case 'R': p->pieces[WHITE][ROOK]   |= bit; break;
				case 'Q': p->pieces[WHITE][QUEEN]  |= bit; break;
				case 'K': p->pieces[WHITE][KING]   |= bit; break;
				case 'p': p->pieces[BLACK][PAWN]   |= bit; break;
				case 'n': p->pieces[BLACK][KNIGHT] |= bit; break;
				case 'b': p->pieces[BLACK][BISHOP] |= bit; break;
				case 'r': p->pieces[BLACK][ROOK]   |= bit; break;
				case 'q': p->pieces[BLACK][QUEEN]  |= bit; break;
				case 'k': p->pieces[BLACK][KING]   |= bit; break;
				default: return false;
				}
				file++;
			}
		}
		if (rank > 0 && *s++ != '/') return false;
	}

	if (*s++ != ' ') return false;
	if (*s == 'w')      p->white_turn = true;
	else if (*s == 'b') p->white_turn = false;
	else return false;
	s++;

	if (*s++ != ' ') return false;
	p->castling = 0;
	if (*s == '-') {
		s++;
	} else {
		while (*s && *s != ' ') {
			switch (*s) {
			case 'K': p->castling |= CASTLE_WK; break;
			case 'Q': p->castling |= CASTLE_WQ; break;
			case 'k': p->castling |= CASTLE_BK; break;
			case 'q': p->castling |= CASTLE_BQ; break;
			default: return false;
			}
			s++;
		}
	}

	if (*s++ != ' ') return false;
	if (*s == '-') {
		p->en_passant = -1;
		s++;
	} else {
		int epf = s[0] - 'a', epr = s[1] - '1';
		if (epf < 0 || epf > 7 || epr < 0 || epr > 7) return false;
		p->en_passant = epr * 8 + epf;
		s += 2;
	}

	if (*s == ' ') {
		s++;
		p->halfmove = 0;
		while (*s >= '0' && *s <= '9')
			p->halfmove = p->halfmove * 10 + (*s++ - '0');
	}
	if (*s == ' ') {
		s++;
		p->fullmove = 0;
		while (*s >= '0' && *s <= '9')
			p->fullmove = p->fullmove * 10 + (*s++ - '0');
	} else {
		p->fullmove = 1;
	}

	p->hash = compute_hash(p);
	return true;
}

void init_position(Position *p) {
	memset(p, 0, sizeof(Position));
	p->pieces[WHITE][PAWN]   = 0x000000000000FF00ULL;
	p->pieces[WHITE][ROOK]   = 0x0000000000000081ULL;
	p->pieces[WHITE][KNIGHT] = 0x0000000000000042ULL;
	p->pieces[WHITE][BISHOP] = 0x0000000000000024ULL;
	p->pieces[WHITE][QUEEN]  = 0x0000000000000008ULL;
	p->pieces[WHITE][KING]   = 0x0000000000000010ULL;
	p->pieces[BLACK][PAWN]   = 0x00FF000000000000ULL;
	p->pieces[BLACK][ROOK]   = 0x8100000000000000ULL;
	p->pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
	p->pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
	p->pieces[BLACK][QUEEN]  = 0x0800000000000000ULL;
	p->pieces[BLACK][KING]   = 0x1000000000000000ULL;
	p->white_turn = true;
	p->castling   = CASTLE_ALL;
	p->en_passant = -1;
	p->halfmove   = 0;
	p->fullmove   = 1;
	p->hash       = compute_hash(p);
}

Bitboard pieces_by_color(const Position *p, Color c) {
	Bitboard r = 0;
	for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
		r |= p->pieces[c][pt];
	return r;
}

Bitboard occupied(const Position *p) {
	return pieces_by_color(p, WHITE) | pieces_by_color(p, BLACK);
}

char piece_at(const Position *p, int sq) {
	Bitboard mask = 1ULL << sq;
	static const char wc[] = "PNBRQK", bc[] = "pnbrqk";
	for (int pt = 0; pt < NUM_PIECE_TYPES; pt++) {
		if (p->pieces[WHITE][pt] & mask) return wc[pt];
		if (p->pieces[BLACK][pt] & mask) return bc[pt];
	}
	return '.';
}

PieceType piece_type_at(const Position *p, int sq) {
	Bitboard mask = 1ULL << sq;
	for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
		if ((p->pieces[WHITE][pt] | p->pieces[BLACK][pt]) & mask)
			return (PieceType)pt;
	return PIECE_NONE;
}

Color piece_color_at(const Position *p, int sq) {
	return (pieces_by_color(p, WHITE) & (1ULL << sq)) ? WHITE : BLACK;
}

void print_board(const Position *p) {
	printf("  a b c d e f g h\n");
	for (int rank = 7; rank >= 0; rank--) {
		printf("%d ", rank + 1);
		for (int file = 0; file < 8; file++)
			printf("%c ", piece_at(p, rank * 8 + file));
		printf("%d\n", rank + 1);
	}
	printf("  a b c d e f g h\n\n");
	printf("%s to move", p->white_turn ? "White" : "Black");
	printf("  Castling: %c%c%c%c",
		(p->castling & CASTLE_WK) ? 'K' : '-',
		(p->castling & CASTLE_WQ) ? 'Q' : '-',
		(p->castling & CASTLE_BK) ? 'k' : '-',
		(p->castling & CASTLE_BQ) ? 'q' : '-');
	if (p->en_passant >= 0)
		printf("  EP: %c%d",
			'a' + SQ_FILE(p->en_passant),
			1 + SQ_RANK(p->en_passant));
	printf("\n\n");
}

bool is_square_attacked(const Position *p, int sq, Color by) {
	Bitboard occ = occupied(p);
	if (knight_attacks(sq) & p->pieces[by][KNIGHT]) return true;
	if (king_attacks(sq) & p->pieces[by][KING]) return true;
	Color opp = (by == WHITE) ? BLACK : WHITE;
	if (pawn_attacks(sq, opp) & p->pieces[by][PAWN]) return true;
	if (bishop_attacks(sq, occ) & (p->pieces[by][BISHOP] | p->pieces[by][QUEEN]))
		return true;
	if (rook_attacks(sq, occ) & (p->pieces[by][ROOK] | p->pieces[by][QUEEN]))
		return true;
	return false;
}

bool is_in_check(const Position *p) {
	Color side = p->white_turn ? WHITE : BLACK;
	Bitboard king = p->pieces[side][KING];
	if (king == 0) return false;
	return is_square_attacked(p, __builtin_ctzll(king),
		p->white_turn ? BLACK : WHITE);
}
