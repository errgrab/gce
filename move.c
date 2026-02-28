#include "move.h"
#include <stddef.h>

void make_move(Position *p, const Move *m) {
	int from = m->from, to = m->to;
	Bitboard from_bb = 1ULL << from, to_bb = 1ULL << to;
	Color side  = p->white_turn ? WHITE : BLACK;
	Color enemy = p->white_turn ? BLACK : WHITE;

	PieceType moved = PIECE_NONE;
	for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
		if (p->pieces[side][pt] & from_bb) { moved = (PieceType)pt; break; }
	if (moved == PIECE_NONE) return;

	uint64_t h = p->hash;
	h ^= zobrist_castling_key(p->castling);
	if (p->en_passant >= 0)
		h ^= zobrist_ep_key(p->en_passant & 7);

	/* Captures */
	if (m->flags == MOVE_CAPTURE || m->flags >= MOVE_PROMO_CAP_N) {
		for (int pt = 0; pt < NUM_PIECE_TYPES; pt++)
			if (p->pieces[enemy][pt] & to_bb) {
				p->pieces[enemy][pt] &= ~to_bb;
				h ^= zobrist_piece_key(enemy, pt, to);
				break;
			}
	}

	/* En passant capture */
	if (m->flags == MOVE_EP_CAPTURE) {
		int cap_sq = (side == WHITE) ? to - 8 : to + 8;
		p->pieces[enemy][PAWN] &= ~(1ULL << cap_sq);
		h ^= zobrist_piece_key(enemy, PAWN, cap_sq);
	}

	/* Move piece */
	p->pieces[side][moved] &= ~from_bb;
	p->pieces[side][moved] |= to_bb;
	h ^= zobrist_piece_key(side, moved, from);
	h ^= zobrist_piece_key(side, moved, to);

	/* Promotion */
	if (MOVE_IS_PROMO(m->flags)) {
		PieceType promo = promo_type_from_flags(m->flags);
		p->pieces[side][moved] &= ~to_bb;
		p->pieces[side][promo] |= to_bb;
		h ^= zobrist_piece_key(side, moved, to);
		h ^= zobrist_piece_key(side, promo, to);
	}

	/* Castling rook */
	if (m->flags == MOVE_CASTLE_K) {
		int rf = (side == WHITE) ? SQ_H1 : SQ_H8;
		int rt = (side == WHITE) ? SQ_F1 : SQ_F8;
		p->pieces[side][ROOK] &= ~(1ULL << rf);
		p->pieces[side][ROOK] |= (1ULL << rt);
		h ^= zobrist_piece_key(side, ROOK, rf);
		h ^= zobrist_piece_key(side, ROOK, rt);
	}
	if (m->flags == MOVE_CASTLE_Q) {
		int rf = (side == WHITE) ? SQ_A1 : SQ_A8;
		int rt = (side == WHITE) ? SQ_D1 : SQ_D8;
		p->pieces[side][ROOK] &= ~(1ULL << rf);
		p->pieces[side][ROOK] |= (1ULL << rt);
		h ^= zobrist_piece_key(side, ROOK, rf);
		h ^= zobrist_piece_key(side, ROOK, rt);
	}

	/* En passant */
	if (m->flags == MOVE_DOUBLE_PUSH)
		p->en_passant = (side == WHITE) ? from + 8 : from - 8;
	else
		p->en_passant = -1;

	/* Castling rights */
	if (from == SQ_E1) p->castling &= ~(CASTLE_WK | CASTLE_WQ);
	if (from == SQ_E8) p->castling &= ~(CASTLE_BK | CASTLE_BQ);
	if (from == SQ_A1 || to == SQ_A1) p->castling &= ~CASTLE_WQ;
	if (from == SQ_H1 || to == SQ_H1) p->castling &= ~CASTLE_WK;
	if (from == SQ_A8 || to == SQ_A8) p->castling &= ~CASTLE_BQ;
	if (from == SQ_H8 || to == SQ_H8) p->castling &= ~CASTLE_BK;

	h ^= zobrist_castling_key(p->castling);
	if (p->en_passant >= 0)
		h ^= zobrist_ep_key(p->en_passant & 7);

	/* Clocks */
	if (moved == PAWN || MOVE_IS_CAPTURE(m->flags))
		p->halfmove = 0;
	else
		p->halfmove++;
	if (side == BLACK) p->fullmove++;

	p->white_turn = !p->white_turn;
	h ^= zobrist_side_key();
	p->hash = h;
}

const char *try_make_move(Position *p, const char *move_str, Move *out_move) {
	Move m;
	if (parse_san(move_str, p, &m)) {
		if (out_move) *out_move = m;
		make_move(p, &m);
		return NULL;
	}
	if (parse_move(move_str, p, &m)) {
		if (out_move) *out_move = m;
		make_move(p, &m);
		return NULL;
	}
	return "Illegal move";
}

static bool insufficient_material(const Position *p) {
	if (p->pieces[WHITE][PAWN] || p->pieces[BLACK][PAWN] ||
	    p->pieces[WHITE][ROOK] || p->pieces[BLACK][ROOK] ||
	    p->pieces[WHITE][QUEEN] || p->pieces[BLACK][QUEEN])
		return false;
	int wn = __builtin_popcountll(p->pieces[WHITE][KNIGHT]);
	int wb = __builtin_popcountll(p->pieces[WHITE][BISHOP]);
	int bn = __builtin_popcountll(p->pieces[BLACK][KNIGHT]);
	int bb = __builtin_popcountll(p->pieces[BLACK][BISHOP]);
	int wm = wn + wb, bm = bn + bb;
	if (wm == 0 && bm == 0) return true;
	if (wm <= 1 && bm == 0) return true;
	if (wm == 0 && bm <= 1) return true;
	return false;
}

GameState get_game_state(const Position *p) {
	if (p->halfmove >= 100) return GAME_DRAW_50;
	if (insufficient_material(p)) return GAME_DRAW_MATERIAL;
	int legal = count_legal_moves(p);
	if (legal == 0)
		return is_in_check(p) ? GAME_CHECKMATE : GAME_STALEMATE;
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
