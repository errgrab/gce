#include "movegen.h"
#include "attack.h"
#include "move.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 * Internal helpers
 * ================================================================ */

static void add_move(MoveList *list, int from, int to, int flags) {
	if (list->count < MAX_MOVES) {
		list->moves[list->count].from  = from;
		list->moves[list->count].to    = to;
		list->moves[list->count].flags = flags;
		list->count++;
	}
}

/* Iterate bits in a bitboard, calling callback for each set bit */
#define FOR_EACH_BIT(bb, sq) \
	for (Bitboard _tmp = (bb); _tmp; _tmp &= _tmp - 1) \
		if (((sq) = __builtin_ctzll(_tmp)), 1)

/* ================================================================
 * Pawn move generation
 * ================================================================ */

static void gen_pawn_moves(const Position *p, MoveList *list, Color side) {
	Bitboard pawns   = (side == WHITE) ? p->wp : p->bp;
	Bitboard enemies = (side == WHITE) ? black_pieces(p) : white_pieces(p);
	Bitboard occ     = occupied(p);
	Bitboard empty   = ~occ;

	int push_dir    = (side == WHITE) ? 8 : -8;
	int promo_rank  = (side == WHITE) ? 7 : 0;   /* rank index where pawn promotes */
	int start_rank  = (side == WHITE) ? 1 : 6;   /* rank index for double push */

	int sq;
	FOR_EACH_BIT(pawns, sq) {
		int rank = square_rank(sq);
		int file = square_file(sq);

		/* ---- Single push ---- */
		int push1 = sq + push_dir;
		if (push1 >= 0 && push1 < 64 && (empty & (1ULL << push1))) {
			if (square_rank(push1) == promo_rank) {
				/* Promotion (all 4 types) */
				add_move(list, sq, push1, MOVE_PROMO_Q);
				add_move(list, sq, push1, MOVE_PROMO_R);
				add_move(list, sq, push1, MOVE_PROMO_B);
				add_move(list, sq, push1, MOVE_PROMO_N);
			} else {
				add_move(list, sq, push1, MOVE_QUIET);
			}

			/* ---- Double push (only if single push was possible) ---- */
			if (rank == start_rank) {
				int push2 = sq + 2 * push_dir;
				if (empty & (1ULL << push2)) {
					add_move(list, sq, push2, MOVE_DOUBLE_PUSH);
				}
			}
		}

		/* ---- Captures ---- */
		Bitboard atk = pawn_attacks(sq, side);

		/* Normal captures */
		Bitboard captures = atk & enemies;
		int csq;
		FOR_EACH_BIT(captures, csq) {
			if (square_rank(csq) == promo_rank) {
				add_move(list, sq, csq, MOVE_PROMO_CAP_Q);
				add_move(list, sq, csq, MOVE_PROMO_CAP_R);
				add_move(list, sq, csq, MOVE_PROMO_CAP_B);
				add_move(list, sq, csq, MOVE_PROMO_CAP_N);
			} else {
				add_move(list, sq, csq, MOVE_CAPTURE);
			}
		}

		/* En passant capture */
		if (p->en_passant >= 0 && (atk & (1ULL << p->en_passant))) {
			add_move(list, sq, p->en_passant, MOVE_EP_CAPTURE);
		}

		(void)file; /* suppress unused warning */
	}
}

/* ================================================================
 * Knight move generation
 * ================================================================ */

static void gen_knight_moves(const Position *p, MoveList *list, Color side) {
	Bitboard knights  = (side == WHITE) ? p->wn : p->bn;
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);

	int sq;
	FOR_EACH_BIT(knights, sq) {
		Bitboard atk = knight_attacks(sq) & ~friendly;
		int tsq;
		FOR_EACH_BIT(atk, tsq) {
			int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
			add_move(list, sq, tsq, flags);
		}
	}
}

/* ================================================================
 * Bishop move generation
 * ================================================================ */

static void gen_bishop_moves(const Position *p, MoveList *list, Color side) {
	Bitboard bishops  = (side == WHITE) ? p->wb : p->bb;
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Bitboard occ      = occupied(p);

	int sq;
	FOR_EACH_BIT(bishops, sq) {
		Bitboard atk = bishop_attacks(sq, occ) & ~friendly;
		int tsq;
		FOR_EACH_BIT(atk, tsq) {
			int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
			add_move(list, sq, tsq, flags);
		}
	}
}

/* ================================================================
 * Rook move generation
 * ================================================================ */

static void gen_rook_moves(const Position *p, MoveList *list, Color side) {
	Bitboard rooks    = (side == WHITE) ? p->wr : p->br;
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Bitboard occ      = occupied(p);

	int sq;
	FOR_EACH_BIT(rooks, sq) {
		Bitboard atk = rook_attacks(sq, occ) & ~friendly;
		int tsq;
		FOR_EACH_BIT(atk, tsq) {
			int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
			add_move(list, sq, tsq, flags);
		}
	}
}

/* ================================================================
 * Queen move generation
 * ================================================================ */

static void gen_queen_moves(const Position *p, MoveList *list, Color side) {
	Bitboard queens   = (side == WHITE) ? p->wq : p->bq;
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Bitboard occ      = occupied(p);

	int sq;
	FOR_EACH_BIT(queens, sq) {
		Bitboard atk = queen_attacks(sq, occ) & ~friendly;
		int tsq;
		FOR_EACH_BIT(atk, tsq) {
			int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
			add_move(list, sq, tsq, flags);
		}
	}
}

/* ================================================================
 * King move generation (including castling)
 * ================================================================ */

static void gen_king_moves(const Position *p, MoveList *list, Color side) {
	Bitboard king     = (side == WHITE) ? p->wk : p->bk;
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Color enemy       = (side == WHITE) ? BLACK : WHITE;

	int ksq = __builtin_ctzll(king);

	/* Normal king moves */
	Bitboard atk = king_attacks(ksq) & ~friendly;
	int tsq;
	FOR_EACH_BIT(atk, tsq) {
		int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
		add_move(list, ksq, tsq, flags);
	}

	/* Castling */
	Bitboard occ = occupied(p);

	if (side == WHITE && !is_square_attacked(p, ksq, enemy)) {
		/* White kingside: e1-g1, f1 and g1 must be empty and not attacked */
		if ((p->castling & CASTLE_WK) &&
		    !(occ & ((1ULL << 5) | (1ULL << 6))) &&
		    !is_square_attacked(p, 5, enemy) &&
		    !is_square_attacked(p, 6, enemy))
		{
			add_move(list, 4, 6, MOVE_CASTLE_K);
		}
		/* White queenside: e1-c1, b1/c1/d1 must be empty, c1/d1 not attacked */
		if ((p->castling & CASTLE_WQ) &&
		    !(occ & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))) &&
		    !is_square_attacked(p, 2, enemy) &&
		    !is_square_attacked(p, 3, enemy))
		{
			add_move(list, 4, 2, MOVE_CASTLE_Q);
		}
	}

	if (side == BLACK && !is_square_attacked(p, ksq, enemy)) {
		/* Black kingside: e8-g8 */
		if ((p->castling & CASTLE_BK) &&
		    !(occ & ((1ULL << 61) | (1ULL << 62))) &&
		    !is_square_attacked(p, 61, enemy) &&
		    !is_square_attacked(p, 62, enemy))
		{
			add_move(list, 60, 62, MOVE_CASTLE_K);
		}
		/* Black queenside: e8-c8 */
		if ((p->castling & CASTLE_BQ) &&
		    !(occ & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59))) &&
		    !is_square_attacked(p, 58, enemy) &&
		    !is_square_attacked(p, 59, enemy))
		{
			add_move(list, 60, 58, MOVE_CASTLE_Q);
		}
	}
}

/* ================================================================
 * Pseudo-legal move generation
 * ================================================================ */

void generate_pseudo_legal(const Position *p, MoveList *list) {
	list->count = 0;
	Color side = p->white_turn ? WHITE : BLACK;

	gen_pawn_moves(p, list, side);
	gen_knight_moves(p, list, side);
	gen_bishop_moves(p, list, side);
	gen_rook_moves(p, list, side);
	gen_queen_moves(p, list, side);
	gen_king_moves(p, list, side);
}

/* ================================================================
 * Legal move generation -- filter pseudo-legal by king safety
 * ================================================================ */

/* Forward declaration: we use make_move (from move.c) to test positions */

void generate_legal_moves(const Position *p, MoveList *list) {
	MoveList pseudo;
	generate_pseudo_legal(p, &pseudo);

	list->count = 0;

	for (int i = 0; i < pseudo.count; i++) {
		Position test;
		copy_position(&test, p);
		make_move(&test, &pseudo.moves[i]);

		/* After making the move, check if our king is in check.
		 * Note: apply_move toggles the turn, so we need to check
		 * the PREVIOUS side's king. */
		Color side = p->white_turn ? WHITE : BLACK;
		Color enemy = p->white_turn ? BLACK : WHITE;
		Bitboard king = (side == WHITE) ? test.wk : test.bk;

		if (king == 0) continue; /* king captured somehow, skip */

		int king_sq = __builtin_ctzll(king);

		if (!is_square_attacked(&test, king_sq, enemy)) {
			list->moves[list->count] = pseudo.moves[i];
			list->count++;
		}
	}
}

/* ================================================================
 * Public query functions
 * ================================================================ */

bool is_move_legal(const Position *p, int from, int to,
                   PieceType promo_piece, Move *out)
{
	MoveList list;
	generate_legal_moves(p, &list);

	for (int i = 0; i < list.count; i++) {
		if (list.moves[i].from == from && list.moves[i].to == to) {
			/* If it's a promotion, match the promotion type */
			if (MOVE_IS_PROMO(list.moves[i].flags)) {
				int want = -1;
				switch (promo_piece) {
					case KNIGHT: want = 0; break;
					case BISHOP: want = 1; break;
					case ROOK:   want = 2; break;
					case QUEEN:  want = 3; break;
					default:     want = 3; break; /* default to queen */
				}
				if ((list.moves[i].flags & 0x03) != want) continue;
			}
			if (out) *out = list.moves[i];
			return true;
		}
	}
	return false;
}

int count_legal_moves(const Position *p) {
	MoveList list;
	generate_legal_moves(p, &list);
	return list.count;
}

/* ================================================================
 * Move string conversion
 * ================================================================ */

void move_to_str(const Move *m, char *buf) {
	buf[0] = 'a' + square_file(m->from);
	buf[1] = '1' + square_rank(m->from);
	buf[2] = 'a' + square_file(m->to);
	buf[3] = '1' + square_rank(m->to);

	if (MOVE_IS_PROMO(m->flags)) {
		const char promo_chars[] = "nbrq";
		buf[4] = promo_chars[m->flags & 0x03];
		buf[5] = '\0';
	} else {
		buf[4] = '\0';
	}
}

bool parse_move(const char *str, const Position *p, Move *m) {
	if (!str || strlen(str) < 4)
		return false;

	char fc = tolower(str[0]);
	char fr = str[1];
	char tc = tolower(str[2]);
	char tr = str[3];

	if (fc < 'a' || fc > 'h') return false;
	if (fr < '1' || fr > '8') return false;
	if (tc < 'a' || tc > 'h') return false;
	if (tr < '1' || tr > '8') return false;

	int from = square_from_coords(fc - 'a', fr - '1');
	int to   = square_from_coords(tc - 'a', tr - '1');

	/* Check for promotion suffix */
	PieceType promo = PIECE_NONE;
	if (strlen(str) >= 5) {
		switch (tolower(str[4])) {
			case 'n': promo = KNIGHT; break;
			case 'b': promo = BISHOP; break;
			case 'r': promo = ROOK;   break;
			case 'q': promo = QUEEN;  break;
			default:  break;
		}
	}

	/* Look up the legal move with correct flags */
	if (!is_move_legal(p, from, to, promo, m))
		return false;

	return true;
}

/* ================================================================
 * SAN (Standard Algebraic Notation) output
 *
 * Format: [Piece][DisambigFile][DisambigRank][x]<DestFile><DestRank>[=Promo][+|#]
 * Pawns: no piece prefix.  Captures by pawns: include source file.
 * Castling: O-O or O-O-O.
 * ================================================================ */

static char piece_to_san_char(PieceType pt) {
	switch (pt) {
		case KNIGHT: return 'N';
		case BISHOP: return 'B';
		case ROOK:   return 'R';
		case QUEEN:  return 'Q';
		case KING:   return 'K';
		default:     return '\0';
	}
}

void move_to_san(const Move *m, const Position *p, char *buf) {
	int idx = 0;

	/* Castling */
	if (m->flags == MOVE_CASTLE_K) {
		strcpy(buf, "O-O");
		idx = 3;
	} else if (m->flags == MOVE_CASTLE_Q) {
		strcpy(buf, "O-O-O");
		idx = 5;
	} else {
		PieceType pt = piece_type_at(p, m->from);
		bool is_capture = MOVE_IS_CAPTURE(m->flags);

		if (pt == PAWN) {
			/* Pawn moves: if capture, include source file */
			if (is_capture) {
				buf[idx++] = 'a' + square_file(m->from);
				buf[idx++] = 'x';
			}
			buf[idx++] = 'a' + square_file(m->to);
			buf[idx++] = '1' + square_rank(m->to);

			/* Promotion suffix */
			if (MOVE_IS_PROMO(m->flags)) {
				buf[idx++] = '=';
				const char promo_chars[] = "NBRQ";
				buf[idx++] = promo_chars[m->flags & 0x03];
			}
		} else {
			/* Non-pawn piece */
			buf[idx++] = piece_to_san_char(pt);

			/* Disambiguation: check if multiple pieces of the same type
			   can reach the same destination */
			MoveList legal;
			generate_legal_moves(p, &legal);

			bool need_file = false;
			bool need_rank = false;
			int ambig_count = 0;

			for (int i = 0; i < legal.count; i++) {
				if (legal.moves[i].to == m->to &&
				    legal.moves[i].from != m->from &&
				    piece_type_at(p, legal.moves[i].from) == pt)
				{
					ambig_count++;
					if (square_file(legal.moves[i].from) == square_file(m->from))
						need_rank = true;
					else
						need_file = true;
				}
			}

			if (ambig_count > 0) {
				if (!need_file && !need_rank) {
					/* Default: file disambiguates */
					need_file = true;
				}
				if (need_file)
					buf[idx++] = 'a' + square_file(m->from);
				if (need_rank)
					buf[idx++] = '1' + square_rank(m->from);
			}

			if (is_capture) buf[idx++] = 'x';

			buf[idx++] = 'a' + square_file(m->to);
			buf[idx++] = '1' + square_rank(m->to);
		}
	}

	/* Check / checkmate suffix: apply the move to a copy and test */
	Position test;
	copy_position(&test, p);

	/* We need to apply the move -- reuse the internal apply logic.
	   We do it by finding the move in the legal list and using make_move
	   from move.h, but to avoid circular dependency, we just inline
	   a quick test. */
	/* Use is_move_legal to get the proper move, then apply it */
	{
		Move validated;
		PieceType promo = PIECE_NONE;
		if (MOVE_IS_PROMO(m->flags)) {
			int pt_idx = m->flags & 0x03;
			PieceType promos[] = { KNIGHT, BISHOP, ROOK, QUEEN };
			promo = promos[pt_idx];
		}
		if (is_move_legal(p, m->from, m->to, promo, &validated)) {
			make_move(&test, &validated);

			int legal_count = count_legal_moves(&test);
			if (legal_count == 0) {
				if (is_in_check(&test))
					buf[idx++] = '#';
			} else if (is_in_check(&test)) {
				buf[idx++] = '+';
			}
		}
	}

	buf[idx] = '\0';
}

/* ================================================================
 * SAN (Standard Algebraic Notation) parser
 *
 * Accepts:  e4  d5  Nf3  Bb5  Qxf7  O-O  O-O-O  exd5  Rae1
 *           e8=Q  e8Q  Nbd2  R1a3  Qh4+  Nf3#  etc.
 *
 * Strategy: extract the piece type, optional disambiguation (file/rank),
 *           whether it's a capture, the destination square, and optional
 *           promotion. Then search the legal move list for a match.
 * ================================================================ */

bool parse_san(const char *str, const Position *p, Move *m) {
	if (!str || str[0] == '\0') return false;

	MoveList legal;
	generate_legal_moves(p, &legal);

	/* Strip trailing +, #, !, ? characters */
	char clean[16];
	int len = 0;
	for (int i = 0; str[i] && i < 14; i++) {
		if (str[i] != '+' && str[i] != '#' && str[i] != '!' && str[i] != '?')
			clean[len++] = str[i];
	}
	clean[len] = '\0';

	/* ---- Castling ---- */
	if (strcmp(clean, "O-O") == 0 || strcmp(clean, "0-0") == 0) {
		for (int i = 0; i < legal.count; i++) {
			if (legal.moves[i].flags == MOVE_CASTLE_K) {
				*m = legal.moves[i];
				return true;
			}
		}
		return false;
	}
	if (strcmp(clean, "O-O-O") == 0 || strcmp(clean, "0-0-0") == 0) {
		for (int i = 0; i < legal.count; i++) {
			if (legal.moves[i].flags == MOVE_CASTLE_Q) {
				*m = legal.moves[i];
				return true;
			}
		}
		return false;
	}

	/* ---- Parse SAN components ---- */
	int ci = 0;  /* cursor index into clean[] */

	PieceType piece = PAWN;
	if (clean[ci] >= 'A' && clean[ci] <= 'Z' && clean[ci] != 'O') {
		switch (clean[ci]) {
			case 'N': piece = KNIGHT; break;
			case 'B': piece = BISHOP; break;
			case 'R': piece = ROOK;   break;
			case 'Q': piece = QUEEN;  break;
			case 'K': piece = KING;   break;
			default: return false;
		}
		ci++;
	}

	/* Now we need to find: [disambig_file][disambig_rank][x]<dest_file><dest_rank>[=promo]
	 * We parse from the END to find the destination square and promotion,
	 * then whatever's left in the middle is disambiguation/capture. */

	/* Check for promotion at the end */
	PieceType promo = PIECE_NONE;
	if (len >= 2) {
		char last = clean[len - 1];
		char before_last = clean[len - 2];

		/* Formats: =Q or just Q at end */
		if (before_last == '=') {
			switch (last) {
				case 'N': promo = KNIGHT; break;
				case 'B': promo = BISHOP; break;
				case 'R': promo = ROOK;   break;
				case 'Q': promo = QUEEN;  break;
				default: return false;
			}
			len -= 2;
			clean[len] = '\0';
		} else if (piece == PAWN && last >= 'A' && last <= 'Z') {
			/* Promotion without = sign, e.g. e8Q */
			switch (last) {
				case 'N': promo = KNIGHT; break;
				case 'B': promo = BISHOP; break;
				case 'R': promo = ROOK;   break;
				case 'Q': promo = QUEEN;  break;
				default: return false;
			}
			len -= 1;
			clean[len] = '\0';
		}
	}

	/* The last two characters must now be the destination square */
	if (len < ci + 2) return false;

	char dest_file_c = clean[len - 2];
	char dest_rank_c = clean[len - 1];

	if (dest_file_c < 'a' || dest_file_c > 'h') return false;
	if (dest_rank_c < '1' || dest_rank_c > '8') return false;

	int dest_file = dest_file_c - 'a';
	int dest_rank = dest_rank_c - '1';
	int dest_sq   = square_from_coords(dest_file, dest_rank);

	/* Whatever is between piece char and destination is disambiguation + 'x' */
	int middle_start = ci;
	int middle_end   = len - 2;

	int disambig_file = -1;  /* 0-7 or -1 if not specified */
	int disambig_rank = -1;  /* 0-7 or -1 if not specified */

	for (int i = middle_start; i < middle_end; i++) {
		if (clean[i] == 'x') continue;  /* skip capture marker */
		if (clean[i] >= 'a' && clean[i] <= 'h') {
			disambig_file = clean[i] - 'a';
		} else if (clean[i] >= '1' && clean[i] <= '8') {
			disambig_rank = clean[i] - '1';
		} else {
			return false;  /* unexpected character */
		}
	}

	/* ---- Search legal moves for a match ---- */
	Move *found = NULL;
	int match_count = 0;

	for (int i = 0; i < legal.count; i++) {
		Move *mv = &legal.moves[i];

		if (mv->to != dest_sq) continue;

		/* Check piece type */
		PieceType src_pt = piece_type_at(p, mv->from);
		if (src_pt != piece) continue;

		/* Check disambiguation */
		if (disambig_file >= 0 && square_file(mv->from) != disambig_file)
			continue;
		if (disambig_rank >= 0 && square_rank(mv->from) != disambig_rank)
			continue;

		/* Check promotion type */
		if (promo != PIECE_NONE) {
			if (!MOVE_IS_PROMO(mv->flags)) continue;
			int mv_promo_type = mv->flags & 0x03;
			int want = -1;
			switch (promo) {
				case KNIGHT: want = 0; break;
				case BISHOP: want = 1; break;
				case ROOK:   want = 2; break;
				case QUEEN:  want = 3; break;
				default: break;
			}
			if (mv_promo_type != want) continue;
		} else {
			/* If not a promotion move, skip promotion entries */
			if (MOVE_IS_PROMO(mv->flags)) continue;
		}

		found = mv;
		match_count++;
	}

	if (match_count == 1 && found) {
		*m = *found;
		return true;
	}

	return false;
}
