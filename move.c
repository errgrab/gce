#include "move.h"
#include <stdio.h>

/* ================================================================
 * apply_move: internal implementation of move execution.
 * Identical logic to the one in movegen.c's static apply_move,
 * but exposed here as the public make_move.
 * ================================================================ */

void make_move(Position *p, const Move *m) {
	int from = m->from;
	int to   = m->to;
	Bitboard from_bb = 1ULL << from;
	Bitboard to_bb   = 1ULL << to;

	Color side = p->white_turn ? WHITE : BLACK;

	/* Identify the piece being moved */
	Bitboard *src_bb = find_piece_bb(p, from);
	if (!src_bb) return;

	/* Handle captures (remove enemy piece at destination) */
	if (m->flags == MOVE_CAPTURE || (m->flags >= MOVE_PROMO_CAP_N)) {
		Bitboard *cap_bb = find_piece_bb(p, to);
		if (cap_bb) {
			*cap_bb &= ~to_bb;
		}
	}

	/* Handle en passant capture */
	if (m->flags == MOVE_EP_CAPTURE) {
		int captured_sq = (side == WHITE) ? to - 8 : to + 8;
		Bitboard *cap_bb = find_piece_bb(p, captured_sq);
		if (cap_bb) {
			*cap_bb &= ~(1ULL << captured_sq);
		}
	}

	/* Move the piece */
	*src_bb &= ~from_bb;
	*src_bb |= to_bb;

	/* Handle promotions */
	if (MOVE_IS_PROMO(m->flags)) {
		*src_bb &= ~to_bb;  /* remove the pawn */
		int promo_type = m->flags & 0x03;
		if (side == WHITE) {
			switch (promo_type) {
				case 0: p->wn |= to_bb; break;
				case 1: p->wb |= to_bb; break;
				case 2: p->wr |= to_bb; break;
				case 3: p->wq |= to_bb; break;
			}
		} else {
			switch (promo_type) {
				case 0: p->bn |= to_bb; break;
				case 1: p->bb |= to_bb; break;
				case 2: p->br |= to_bb; break;
				case 3: p->bq |= to_bb; break;
			}
		}
	}

	/* Handle castling: move the rook */
	if (m->flags == MOVE_CASTLE_K) {
		if (side == WHITE) {
			p->wr &= ~(1ULL << 7);
			p->wr |= (1ULL << 5);
		} else {
			p->br &= ~(1ULL << 63);
			p->br |= (1ULL << 61);
		}
	}
	if (m->flags == MOVE_CASTLE_Q) {
		if (side == WHITE) {
			p->wr &= ~(1ULL << 0);
			p->wr |= (1ULL << 3);
		} else {
			p->br &= ~(1ULL << 56);
			p->br |= (1ULL << 59);
		}
	}

	/* Update en passant square */
	if (m->flags == MOVE_DOUBLE_PUSH) {
		p->en_passant = (side == WHITE) ? from + 8 : from - 8;
	} else {
		p->en_passant = -1;
	}

	/* Update castling rights */
	if (from == 4)  p->castling &= ~(CASTLE_WK | CASTLE_WQ);
	if (from == 60) p->castling &= ~(CASTLE_BK | CASTLE_BQ);
	if (from == 0  || to == 0)  p->castling &= ~CASTLE_WQ;
	if (from == 7  || to == 7)  p->castling &= ~CASTLE_WK;
	if (from == 56 || to == 56) p->castling &= ~CASTLE_BQ;
	if (from == 63 || to == 63) p->castling &= ~CASTLE_BK;

	/* Update halfmove clock */
	/* Check if it was a pawn move or capture */
	PieceType moved = piece_type_at(p, to);
	if (moved == PAWN || MOVE_IS_CAPTURE(m->flags)) {
		p->halfmove = 0;
	} else {
		p->halfmove++;
	}

	/* Update fullmove counter */
	if (side == BLACK) {
		p->fullmove++;
	}

	/* Toggle turn */
	p->white_turn = !p->white_turn;

	/* Update Zobrist hash */
	p->hash = compute_hash(p);
}

/* ================================================================
 * try_make_move: parse + validate + execute
 * ================================================================ */

const char *try_make_move(Position *p, const char *move_str, Move *out_move) {
	Move m;

	/* Try SAN first (e.g. "e4", "Nf3", "O-O", "exd5") */
	if (parse_san(move_str, p, &m)) {
		if (out_move) *out_move = m;
		make_move(p, &m);
		return NULL;
	}

	/* Fall back to coordinate notation (e.g. "e2e4", "e7e8q") */
	if (parse_move(move_str, p, &m)) {
		if (out_move) *out_move = m;
		make_move(p, &m);
		return NULL;
	}

	return "Illegal move";
}

/* ================================================================
 * Game state detection
 * ================================================================ */

static bool insufficient_material(const Position *p) {
	/* If any pawns, rooks, or queens exist: sufficient */
	if (p->wp || p->bp || p->wr || p->br || p->wq || p->bq)
		return false;

	int wn = __builtin_popcountll(p->wn);
	int wb = __builtin_popcountll(p->wb);
	int bn = __builtin_popcountll(p->bn);
	int bb = __builtin_popcountll(p->bb);

	int w_minor = wn + wb;
	int b_minor = bn + bb;

	/* K vs K */
	if (w_minor == 0 && b_minor == 0) return true;
	/* K+minor vs K */
	if (w_minor <= 1 && b_minor == 0) return true;
	if (w_minor == 0 && b_minor <= 1) return true;

	return false;
}

GameState get_game_state(const Position *p) {
	/* 50-move rule */
	if (p->halfmove >= 100)
		return GAME_DRAW_50;

	/* Insufficient material */
	if (insufficient_material(p))
		return GAME_DRAW_MATERIAL;

	/* Check for checkmate / stalemate */
	int legal = count_legal_moves(p);
	if (legal == 0) {
		if (is_in_check(p))
			return GAME_CHECKMATE;
		else
			return GAME_STALEMATE;
	}

	return GAME_ONGOING;
}

const char *game_state_str(GameState state) {
	switch (state) {
		case GAME_ONGOING:       return "Game in progress";
		case GAME_CHECKMATE:     return "Checkmate";
		case GAME_STALEMATE:     return "Stalemate";
		case GAME_DRAW_50:       return "Draw (50-move rule)";
		case GAME_DRAW_MATERIAL: return "Draw (insufficient material)";
		default:                 return "Unknown";
	}
}
