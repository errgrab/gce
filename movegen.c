#include "movegen.h"
#include "attack.h"
#include "move.h"
#include <string.h>
#include <ctype.h>

static void add_move(MoveList *list, int from, int to, int flags) {
	if (list->count < MAX_MOVES) {
		list->moves[list->count].from  = from;
		list->moves[list->count].to    = to;
		list->moves[list->count].flags = flags;
		list->count++;
	}
}

#define FOR_EACH_BIT(bb, sq) \
	for (Bitboard _tmp = (bb); _tmp; _tmp &= _tmp - 1) \
		if (((sq) = __builtin_ctzll(_tmp)), 1)

static void gen_pawn_moves(const Position *p, MoveList *list, Color side) {
	Bitboard pawns   = p->pieces[side][PAWN];
	Bitboard enemies = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Bitboard empty   = ~occupied(p);
	int push_dir   = (side == WHITE) ? 8 : -8;
	int promo_rank = (side == WHITE) ? 7 : 0;
	int start_rank = (side == WHITE) ? 1 : 6;
	int sq;

	FOR_EACH_BIT(pawns, sq) {
		int rank = SQ_RANK(sq);
		int push1 = sq + push_dir;
		if (push1 >= 0 && push1 < 64 && (empty & (1ULL << push1))) {
			if (SQ_RANK(push1) == promo_rank) {
				add_move(list, sq, push1, MOVE_PROMO_Q);
				add_move(list, sq, push1, MOVE_PROMO_R);
				add_move(list, sq, push1, MOVE_PROMO_B);
				add_move(list, sq, push1, MOVE_PROMO_N);
			} else {
				add_move(list, sq, push1, MOVE_QUIET);
			}
			if (rank == start_rank) {
				int push2 = sq + 2 * push_dir;
				if (empty & (1ULL << push2))
					add_move(list, sq, push2, MOVE_DOUBLE_PUSH);
			}
		}
		Bitboard atk = pawn_attacks(sq, side);
		Bitboard captures = atk & enemies;
		int csq;
		FOR_EACH_BIT(captures, csq) {
			if (SQ_RANK(csq) == promo_rank) {
				add_move(list, sq, csq, MOVE_PROMO_CAP_Q);
				add_move(list, sq, csq, MOVE_PROMO_CAP_R);
				add_move(list, sq, csq, MOVE_PROMO_CAP_B);
				add_move(list, sq, csq, MOVE_PROMO_CAP_N);
			} else {
				add_move(list, sq, csq, MOVE_CAPTURE);
			}
		}
		if (p->en_passant >= 0 && (atk & (1ULL << p->en_passant)))
			add_move(list, sq, p->en_passant, MOVE_EP_CAPTURE);
	}
}

static void gen_piece_moves(const Position *p, MoveList *list, Color side,
                            PieceType pt,
                            Bitboard (*atk_fn)(int, Bitboard)) {
	Bitboard pcs      = p->pieces[side][pt];
	Bitboard friendly = pieces_by_color(p, side);
	Bitboard enemies  = pieces_by_color(p, side == WHITE ? BLACK : WHITE);
	Bitboard occ      = occupied(p);
	int sq;
	FOR_EACH_BIT(pcs, sq) {
		Bitboard atk = atk_fn(sq, occ) & ~friendly;
		int tsq;
		FOR_EACH_BIT(atk, tsq) {
			int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
			add_move(list, sq, tsq, flags);
		}
	}
}

static void gen_knight_moves(const Position *p, MoveList *list, Color side) {
	Bitboard knights  = p->pieces[side][KNIGHT];
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

static void gen_king_moves(const Position *p, MoveList *list, Color side) {
	Bitboard king = p->pieces[side][KING];
	if (king == 0) return;
	Bitboard friendly = pieces_by_color(p, side);
	Color enemy       = (side == WHITE) ? BLACK : WHITE;
	Bitboard enemies  = pieces_by_color(p, enemy);
	int ksq = __builtin_ctzll(king);

	Bitboard atk = king_attacks(ksq) & ~friendly;
	int tsq;
	FOR_EACH_BIT(atk, tsq) {
		int flags = (enemies & (1ULL << tsq)) ? MOVE_CAPTURE : MOVE_QUIET;
		add_move(list, ksq, tsq, flags);
	}

	Bitboard occ = occupied(p);
	if (side == WHITE && !is_square_attacked(p, ksq, enemy)) {
		if ((p->castling & CASTLE_WK) &&
		    !(occ & ((1ULL << SQ_F1) | (1ULL << SQ_G1))) &&
		    !is_square_attacked(p, SQ_F1, enemy) &&
		    !is_square_attacked(p, SQ_G1, enemy))
			add_move(list, SQ_E1, SQ_G1, MOVE_CASTLE_K);
		if ((p->castling & CASTLE_WQ) &&
		    !(occ & ((1ULL << SQ_B1) | (1ULL << SQ_C1) | (1ULL << SQ_D1))) &&
		    !is_square_attacked(p, SQ_C1, enemy) &&
		    !is_square_attacked(p, SQ_D1, enemy))
			add_move(list, SQ_E1, SQ_C1, MOVE_CASTLE_Q);
	}
	if (side == BLACK && !is_square_attacked(p, ksq, enemy)) {
		if ((p->castling & CASTLE_BK) &&
		    !(occ & ((1ULL << SQ_F8) | (1ULL << SQ_G8))) &&
		    !is_square_attacked(p, SQ_F8, enemy) &&
		    !is_square_attacked(p, SQ_G8, enemy))
			add_move(list, SQ_E8, SQ_G8, MOVE_CASTLE_K);
		if ((p->castling & CASTLE_BQ) &&
		    !(occ & ((1ULL << SQ_B8) | (1ULL << SQ_C8) | (1ULL << SQ_D8))) &&
		    !is_square_attacked(p, SQ_C8, enemy) &&
		    !is_square_attacked(p, SQ_D8, enemy))
			add_move(list, SQ_E8, SQ_C8, MOVE_CASTLE_Q);
	}
}

void generate_pseudo_legal(const Position *p, MoveList *list) {
	list->count = 0;
	Color side = p->white_turn ? WHITE : BLACK;
	gen_pawn_moves(p, list, side);
	gen_knight_moves(p, list, side);
	gen_piece_moves(p, list, side, BISHOP, bishop_attacks);
	gen_piece_moves(p, list, side, ROOK, rook_attacks);
	gen_piece_moves(p, list, side, QUEEN, queen_attacks);
	gen_king_moves(p, list, side);
}

static void filter_legal(const Position *p, const MoveList *pseudo,
                         MoveList *out, bool captures_only) {
	Color side  = p->white_turn ? WHITE : BLACK;
	Color enemy = p->white_turn ? BLACK : WHITE;
	out->count  = 0;
	for (int i = 0; i < pseudo->count; i++) {
		if (captures_only && !MOVE_IS_CAPTURE(pseudo->moves[i].flags))
			continue;
		Position test = *p;
		make_move(&test, &pseudo->moves[i]);
		Bitboard king = test.pieces[side][KING];
		if (king == 0) continue;
		if (!is_square_attacked(&test, __builtin_ctzll(king), enemy)) {
			out->moves[out->count] = pseudo->moves[i];
			out->count++;
		}
	}
}

void generate_legal_moves(const Position *p, MoveList *list) {
	MoveList pseudo;
	generate_pseudo_legal(p, &pseudo);
	filter_legal(p, &pseudo, list, false);
}

void generate_legal_captures(const Position *p, MoveList *list) {
	MoveList pseudo;
	generate_pseudo_legal(p, &pseudo);
	filter_legal(p, &pseudo, list, true);
}

bool is_move_legal(const Position *p, int from, int to,
                   PieceType promo_piece, Move *out) {
	MoveList list;
	generate_legal_moves(p, &list);
	for (int i = 0; i < list.count; i++) {
		if (list.moves[i].from == from && list.moves[i].to == to) {
			if (MOVE_IS_PROMO(list.moves[i].flags)) {
				PieceType actual = promo_type_from_flags(list.moves[i].flags);
				PieceType want = (promo_piece == PIECE_NONE) ? QUEEN : promo_piece;
				if (actual != want) continue;
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

void move_to_str(const Move *m, char *buf) {
	buf[0] = 'a' + SQ_FILE(m->from);
	buf[1] = '1' + SQ_RANK(m->from);
	buf[2] = 'a' + SQ_FILE(m->to);
	buf[3] = '1' + SQ_RANK(m->to);
	if (MOVE_IS_PROMO(m->flags)) {
		buf[4] = "nbrq"[m->flags & PROMO_PIECE_MASK];
		buf[5] = '\0';
	} else {
		buf[4] = '\0';
	}
}

bool parse_move(const char *str, const Position *p, Move *m) {
	if (!str || strlen(str) < 4) return false;
	char fc = tolower(str[0]), fr = str[1];
	char tc = tolower(str[2]), tr = str[3];
	if (fc < 'a' || fc > 'h' || fr < '1' || fr > '8') return false;
	if (tc < 'a' || tc > 'h' || tr < '1' || tr > '8') return false;
	int from = (fr - '1') * 8 + (fc - 'a');
	int to   = (tr - '1') * 8 + (tc - 'a');
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
	return is_move_legal(p, from, to, promo, m);
}

void move_to_san(const Move *m, const Position *p, char *buf) {
	int idx = 0;

	if (m->flags == MOVE_CASTLE_K) {
		strcpy(buf, "O-O"); idx = 3;
	} else if (m->flags == MOVE_CASTLE_Q) {
		strcpy(buf, "O-O-O"); idx = 5;
	} else {
		PieceType pt = piece_type_at(p, m->from);
		bool is_cap = MOVE_IS_CAPTURE(m->flags);

		if (pt == PAWN) {
			if (is_cap) {
				buf[idx++] = 'a' + SQ_FILE(m->from);
				buf[idx++] = 'x';
			}
			buf[idx++] = 'a' + SQ_FILE(m->to);
			buf[idx++] = '1' + SQ_RANK(m->to);
			if (MOVE_IS_PROMO(m->flags)) {
				buf[idx++] = '=';
				buf[idx++] = "NBRQ"[m->flags & PROMO_PIECE_MASK];
			}
		} else {
			static const char pc[] = {0, 'N', 'B', 'R', 'Q', 'K'};
			buf[idx++] = pc[pt];

			/* Disambiguation */
			MoveList legal;
			generate_legal_moves(p, &legal);
			bool need_file = false, need_rank = false;
			int ambig = 0;
			for (int i = 0; i < legal.count; i++) {
				if (legal.moves[i].to == m->to &&
				    legal.moves[i].from != m->from &&
				    piece_type_at(p, legal.moves[i].from) == pt) {
					ambig++;
					if (SQ_FILE(legal.moves[i].from) == SQ_FILE(m->from))
						need_rank = true;
					else
						need_file = true;
				}
			}
			if (ambig > 0) {
				if (!need_file && !need_rank) need_file = true;
				if (need_file) buf[idx++] = 'a' + SQ_FILE(m->from);
				if (need_rank) buf[idx++] = '1' + SQ_RANK(m->from);
			}
			if (is_cap) buf[idx++] = 'x';
			buf[idx++] = 'a' + SQ_FILE(m->to);
			buf[idx++] = '1' + SQ_RANK(m->to);
		}
	}

	/* Check/checkmate suffix */
	{
		Position test = *p;
		make_move(&test, m);
		int lc = count_legal_moves(&test);
		if (lc == 0 && is_in_check(&test))
			buf[idx++] = '#';
		else if (is_in_check(&test))
			buf[idx++] = '+';
	}
	buf[idx] = '\0';
}

bool parse_san(const char *str, const Position *p, Move *m) {
	if (!str || str[0] == '\0') return false;

	MoveList legal;
	generate_legal_moves(p, &legal);

	/* Strip trailing +, #, !, ? */
	char clean[16];
	int len = 0;
	for (int i = 0; str[i] && i < 14; i++)
		if (str[i] != '+' && str[i] != '#' && str[i] != '!' && str[i] != '?')
			clean[len++] = str[i];
	clean[len] = '\0';

	/* Castling */
	if (strcmp(clean, "O-O") == 0 || strcmp(clean, "0-0") == 0) {
		for (int i = 0; i < legal.count; i++)
			if (legal.moves[i].flags == MOVE_CASTLE_K) {
				*m = legal.moves[i]; return true;
			}
		return false;
	}
	if (strcmp(clean, "O-O-O") == 0 || strcmp(clean, "0-0-0") == 0) {
		for (int i = 0; i < legal.count; i++)
			if (legal.moves[i].flags == MOVE_CASTLE_Q) {
				*m = legal.moves[i]; return true;
			}
		return false;
	}

	int ci = 0;
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

	/* Check for promotion at end */
	PieceType promo = PIECE_NONE;
	if (len >= 2) {
		char last = clean[len - 1], prev = clean[len - 2];
		if (prev == '=') {
			switch (last) {
			case 'N': promo = KNIGHT; break;
			case 'B': promo = BISHOP; break;
			case 'R': promo = ROOK;   break;
			case 'Q': promo = QUEEN;  break;
			default: return false;
			}
			len -= 2; clean[len] = '\0';
		} else if (piece == PAWN && last >= 'A' && last <= 'Z') {
			switch (last) {
			case 'N': promo = KNIGHT; break;
			case 'B': promo = BISHOP; break;
			case 'R': promo = ROOK;   break;
			case 'Q': promo = QUEEN;  break;
			default: return false;
			}
			len -= 1; clean[len] = '\0';
		}
	}

	if (len < ci + 2) return false;
	char df = clean[len - 2], dr = clean[len - 1];
	if (df < 'a' || df > 'h' || dr < '1' || dr > '8') return false;
	int dest_sq = (dr - '1') * 8 + (df - 'a');

	int disambig_file = -1, disambig_rank = -1;
	for (int i = ci; i < len - 2; i++) {
		if (clean[i] == 'x') continue;
		if (clean[i] >= 'a' && clean[i] <= 'h')
			disambig_file = clean[i] - 'a';
		else if (clean[i] >= '1' && clean[i] <= '8')
			disambig_rank = clean[i] - '1';
		else return false;
	}

	Move *found = NULL;
	int match = 0;
	for (int i = 0; i < legal.count; i++) {
		Move *mv = &legal.moves[i];
		if (mv->to != dest_sq) continue;
		if (piece_type_at(p, mv->from) != piece) continue;
		if (disambig_file >= 0 && SQ_FILE(mv->from) != disambig_file) continue;
		if (disambig_rank >= 0 && SQ_RANK(mv->from) != disambig_rank) continue;
		if (promo != PIECE_NONE) {
			if (!MOVE_IS_PROMO(mv->flags)) continue;
			if (promo_type_from_flags(mv->flags) != promo) continue;
		} else {
			if (MOVE_IS_PROMO(mv->flags)) continue;
		}
		found = mv;
		match++;
	}
	if (match == 1 && found) { *m = *found; return true; }
	return false;
}
