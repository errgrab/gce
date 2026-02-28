#define _POSIX_C_SOURCE 200809L
#include "engine.h"
#include "move.h"
#include "attack.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define VAL_PAWN   100
#define VAL_KNIGHT 320
#define VAL_BISHOP 330
#define VAL_ROOK   500
#define VAL_QUEEN  900
#define VAL_KING   20000

static const int piece_value[7] = {
	VAL_PAWN, VAL_KNIGHT, VAL_BISHOP, VAL_ROOK, VAL_QUEEN, VAL_KING, 0
};

static const int pst_pawn[64] = {
	  0,   0,   0,   0,   0,   0,   0,   0,
	  5,  10,  10, -20, -20,  10,  10,   5,
	  5,  -5, -10,   0,   0, -10,  -5,   5,
	  0,   0,   0,  20,  20,   0,   0,   0,
	  5,   5,  10,  25,  25,  10,   5,   5,
	 10,  10,  20,  30,  30,  20,  10,  10,
	 50,  50,  50,  50,  50,  50,  50,  50,
	  0,   0,   0,   0,   0,   0,   0,   0
};
static const int pst_knight[64] = {
	-50, -40, -30, -30, -30, -30, -40, -50,
	-40, -20,   0,   5,   5,   0, -20, -40,
	-30,   5,  10,  15,  15,  10,   5, -30,
	-30,   0,  15,  20,  20,  15,   0, -30,
	-30,   5,  15,  20,  20,  15,   5, -30,
	-30,   0,  10,  15,  15,  10,   0, -30,
	-40, -20,   0,   0,   0,   0, -20, -40,
	-50, -40, -30, -30, -30, -30, -40, -50
};
static const int pst_bishop[64] = {
	-20, -10, -10, -10, -10, -10, -10, -20,
	-10,   5,   0,   0,   0,   0,   5, -10,
	-10,  10,  10,  10,  10,  10,  10, -10,
	-10,   0,  10,  10,  10,  10,   0, -10,
	-10,   5,   5,  10,  10,   5,   5, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-20, -10, -10, -10, -10, -10, -10, -20
};
static const int pst_rook[64] = {
	  0,   0,   0,   5,   5,   0,   0,   0,
	 -5,   0,   0,   0,   0,   0,   0,  -5,
	 -5,   0,   0,   0,   0,   0,   0,  -5,
	 -5,   0,   0,   0,   0,   0,   0,  -5,
	 -5,   0,   0,   0,   0,   0,   0,  -5,
	 -5,   0,   0,   0,   0,   0,   0,  -5,
	  5,  10,  10,  10,  10,  10,  10,   5,
	  0,   0,   0,   0,   0,   0,   0,   0
};
static const int pst_queen[64] = {
	-20, -10, -10,  -5,  -5, -10, -10, -20,
	-10,   0,   5,   0,   0,   0,   0, -10,
	-10,   5,   5,   5,   5,   5,   0, -10,
	  0,   0,   5,   5,   5,   5,   0,  -5,
	 -5,   0,   5,   5,   5,   5,   0,  -5,
	-10,   0,   5,   5,   5,   5,   0, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-20, -10, -10,  -5,  -5, -10, -10, -20
};
static const int pst_king_mg[64] = {
	 20,  30,  10,   0,   0,  10,  30,  20,
	 20,  20,   0,   0,   0,   0,  20,  20,
	-10, -20, -20, -20, -20, -20, -20, -10,
	-20, -30, -30, -40, -40, -30, -30, -20,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30
};

static const int *pst_tables[NUM_PIECE_TYPES] = {
	pst_pawn, pst_knight, pst_bishop, pst_rook, pst_queen, pst_king_mg
};

static const Bitboard file_mask[8] = {
	0x0101010101010101ULL, 0x0202020202020202ULL,
	0x0404040404040404ULL, 0x0808080808080808ULL,
	0x1010101010101010ULL, 0x2020202020202020ULL,
	0x4040404040404040ULL, 0x8080808080808080ULL
};

static const Bitboard rank_mask[8] = {
	0x00000000000000FFULL, 0x000000000000FF00ULL,
	0x0000000000FF0000ULL, 0x00000000FF000000ULL,
	0x000000FF00000000ULL, 0x0000FF0000000000ULL,
	0x00FF000000000000ULL, 0xFF00000000000000ULL
};

static int pst_sum(Bitboard bb, const int *table) {
	int score = 0;
	while (bb) {
		int sq = __builtin_ctzll(bb);
		score += table[sq];
		bb &= bb - 1;
	}
	return score;
}

static int pst_sum_mirror(Bitboard bb, const int *table) {
	int score = 0;
	while (bb) {
		int sq = __builtin_ctzll(bb);
		score += table[(7 - (sq >> 3)) * 8 + (sq & 7)];
		bb &= bb - 1;
	}
	return score;
}

static int eval_pawns(const Position *p, Color c) {
	Bitboard pawns = p->pieces[c][PAWN];
	Bitboard enemy = p->pieces[c ^ 1][PAWN];
	int score = 0;
	Bitboard bb = pawns;
	while (bb) {
		int sq = __builtin_ctzll(bb);
		bb &= bb - 1;
		int f = sq & 7;
		/* Doubled: another friendly pawn on the same file */
		if (pawns & file_mask[f] & ~(1ULL << sq))
			score -= 10;
		/* Isolated: no friendly pawns on adjacent files */
		Bitboard adj = 0;
		if (f > 0) adj |= file_mask[f - 1];
		if (f < 7) adj |= file_mask[f + 1];
		if (!(pawns & adj))
			score -= 15;
		/* Passed: no enemy pawns on same or adjacent files ahead */
		Bitboard front = 0;
		if (c == WHITE) {
			for (int r = (sq >> 3) + 1; r <= 7; r++)
				front |= rank_mask[r];
		} else {
			for (int r = (sq >> 3) - 1; r >= 0; r--)
				front |= rank_mask[r];
		}
		Bitboard block_files = file_mask[f] | adj;
		if (!(enemy & block_files & front)) {
			int rank = (c == WHITE) ? (sq >> 3) : (7 - (sq >> 3));
			score += 10 + rank * rank;
		}
	}
	return score;
}

static int eval_king_safety(const Position *p, Color c) {
	Bitboard king = p->pieces[c][KING];
	if (!king) return 0;
	int ksq = __builtin_ctzll(king);
	int kf = ksq & 7;
	int score = 0;
	Bitboard pawns = p->pieces[c][PAWN];
	/* Pawn shield: pawns on files near king */
	for (int df = -1; df <= 1; df++) {
		int f = kf + df;
		if (f < 0 || f > 7) continue;
		Bitboard fpawns = pawns & file_mask[f];
		if (fpawns) {
			int closest;
			if (c == WHITE) {
				closest = __builtin_ctzll(fpawns) >> 3;
				int dist = closest - (ksq >> 3);
				if (dist >= 1 && dist <= 2) score += 10;
			} else {
				closest = (63 - __builtin_clzll(fpawns)) >> 3;
				int dist = (ksq >> 3) - closest;
				if (dist >= 1 && dist <= 2) score += 10;
			}
		} else {
			score -= 15;
		}
	}
	return score;
}

static int eval_mobility(const Position *p, Color c) {
	Bitboard occ = occupied(p);
	Bitboard friendly = pieces_by_color(p, c);
	int mob = 0;
	Bitboard bb;
	bb = p->pieces[c][KNIGHT];
	while (bb) {
		int sq = __builtin_ctzll(bb);
		bb &= bb - 1;
		mob += __builtin_popcountll(knight_attacks(sq) & ~friendly);
	}
	bb = p->pieces[c][BISHOP];
	while (bb) {
		int sq = __builtin_ctzll(bb);
		bb &= bb - 1;
		mob += __builtin_popcountll(bishop_attacks(sq, occ) & ~friendly);
	}
	bb = p->pieces[c][ROOK];
	while (bb) {
		int sq = __builtin_ctzll(bb);
		bb &= bb - 1;
		mob += __builtin_popcountll(rook_attacks(sq, occ) & ~friendly);
	}
	bb = p->pieces[c][QUEEN];
	while (bb) {
		int sq = __builtin_ctzll(bb);
		bb &= bb - 1;
		mob += __builtin_popcountll(queen_attacks(sq, occ) & ~friendly);
	}
	return mob * 3;
}

static int eval_rooks(const Position *p, Color c) {
	Bitboard rooks = p->pieces[c][ROOK];
	Bitboard our_pawns = p->pieces[c][PAWN];
	Bitboard their_pawns = p->pieces[c ^ 1][PAWN];
	int score = 0;
	while (rooks) {
		int sq = __builtin_ctzll(rooks);
		rooks &= rooks - 1;
		int f = sq & 7;
		if (!(our_pawns & file_mask[f])) {
			if (!(their_pawns & file_mask[f]))
				score += 20;
			else
				score += 10;
		}
	}
	return score;
}

int evaluate(const Position *p) {
	int score = 0;
	for (int pt = PAWN; pt <= QUEEN; pt++)
		score += piece_value[pt]
		       * (__builtin_popcountll(p->pieces[WHITE][pt])
		        - __builtin_popcountll(p->pieces[BLACK][pt]));
	if (__builtin_popcountll(p->pieces[WHITE][BISHOP]) >= 2) score += 30;
	if (__builtin_popcountll(p->pieces[BLACK][BISHOP]) >= 2) score -= 30;
	for (int pt = 0; pt < NUM_PIECE_TYPES; pt++) {
		score += pst_sum(p->pieces[WHITE][pt], pst_tables[pt]);
		score -= pst_sum_mirror(p->pieces[BLACK][pt], pst_tables[pt]);
	}
	score += eval_pawns(p, WHITE) - eval_pawns(p, BLACK);
	score += eval_king_safety(p, WHITE) - eval_king_safety(p, BLACK);
	score += eval_mobility(p, WHITE) - eval_mobility(p, BLACK);
	score += eval_rooks(p, WHITE) - eval_rooks(p, BLACK);
	return score;
}

/* Transposition table */
#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2

typedef struct {
	uint64_t key;
	int score, depth, flag;
	Move best_move;
} TTEntry;

#define TT_SIZE (1 << 20)
#define TT_MASK (TT_SIZE - 1)

static TTEntry tt[TT_SIZE];
static Move killers[MAX_PLY][2];
static int history[2][64][64];

void engine_init(void) {
	memset(tt, 0, sizeof(tt));
	memset(killers, 0, sizeof(killers));
	memset(history, 0, sizeof(history));
}

volatile int engine_stop = 0;
EngineCheckFn engine_check_fn = NULL;

static uint64_t nodes;
static int64_t search_start_time;
static int64_t search_time_limit;

static int64_t get_time_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void check_limits(void) {
	if (search_time_limit > 0 &&
	    get_time_ms() - search_start_time >= search_time_limit)
		engine_stop = 1;
	if (engine_check_fn) engine_check_fn();
}

static TTEntry *tt_probe(uint64_t key) {
	TTEntry *e = &tt[key & TT_MASK];
	return (e->key == key) ? e : NULL;
}

static void tt_store(uint64_t key, int score, int depth, int flag, Move best) {
	TTEntry *e = &tt[key & TT_MASK];
	if (e->key == 0 || e->key == key || depth >= e->depth) {
		e->key = key;
		e->score = score;
		e->depth = depth;
		e->flag = flag;
		e->best_move = best;
	}
}

static int score_move(const Position *p, const Move *m,
                      const Move *tt_move, int ply) {
	if (tt_move && m->from == tt_move->from && m->to == tt_move->to
	    && m->flags == tt_move->flags)
		return 100000;
	if (MOVE_IS_CAPTURE(m->flags)) {
		PieceType victim = (m->flags == MOVE_EP_CAPTURE)
			? PAWN : piece_type_at(p, m->to);
		return 50000 + piece_value[victim] * 10 - piece_value[piece_type_at(p, m->from)];
	}
	if (MOVE_IS_PROMO(m->flags)) return 48000;
	if (ply < MAX_PLY) {
		if (m->from == killers[ply][0].from && m->to == killers[ply][0].to)
			return 40000;
		if (m->from == killers[ply][1].from && m->to == killers[ply][1].to)
			return 39000;
	}
	return history[p->white_turn ? 0 : 1][m->from][m->to];
}

static void score_moves(const Position *p, MoveList *list,
                        int *scores, const Move *tt_move, int ply) {
	for (int i = 0; i < list->count; i++)
		scores[i] = score_move(p, &list->moves[i], tt_move, ply);
}

static void pick_best(MoveList *list, int *scores, int start) {
	int best = start;
	for (int i = start + 1; i < list->count; i++)
		if (scores[i] > scores[best]) best = i;
	if (best != start) {
		Move tm = list->moves[start];
		list->moves[start] = list->moves[best];
		list->moves[best] = tm;
		int ts = scores[start];
		scores[start] = scores[best];
		scores[best] = ts;
	}
}

static void store_killer(const Move *m, int ply) {
	if (ply >= MAX_PLY) return;
	if (killers[ply][0].from == m->from && killers[ply][0].to == m->to) return;
	killers[ply][1] = killers[ply][0];
	killers[ply][0] = *m;
}

static void update_history(const Position *p, const Move *m, int depth) {
	int c = p->white_turn ? 0 : 1;
	history[c][m->from][m->to] += depth * depth;
	if (history[c][m->from][m->to] > 30000)
		history[c][m->from][m->to] = 30000;
}

static int quiescence(const Position *p, int alpha, int beta) {
	nodes++;
	if (engine_stop) return 0;

	int eval = evaluate(p);
	if (!p->white_turn) eval = -eval;
	if (eval >= beta) return beta;
	if (eval > alpha) alpha = eval;

	MoveList caps;
	generate_legal_captures(p, &caps);
	int scores[MAX_MOVES];
	score_moves(p, &caps, scores, NULL, MAX_PLY);

	for (int i = 0; i < caps.count; i++) {
		pick_best(&caps, scores, i);
		PieceType victim = piece_type_at(p, caps.moves[i].to);
		if (caps.moves[i].flags == MOVE_EP_CAPTURE) victim = PAWN;
		if (eval + piece_value[victim] + 200 < alpha
		    && !MOVE_IS_PROMO(caps.moves[i].flags))
			continue;
		Position child = *p;
		make_move(&child, &caps.moves[i]);
		int score = -quiescence(&child, -beta, -alpha);
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	return alpha;
}

static int negamax(const Position *p, int depth, int alpha, int beta,
                   int ply, Move *best_move, bool do_null) {
	nodes++;
	if ((nodes & 4095) == 0) check_limits();
	if (engine_stop) return 0;
	if (p->halfmove >= 100) return 0;

	bool pv_node = (beta - alpha > 1);
	int orig_alpha = alpha;
	TTEntry *tt_entry = tt_probe(p->hash);
	Move tt_mv;
	Move *tt_move = NULL;

	if (tt_entry) {
		tt_mv = tt_entry->best_move;
		tt_move = &tt_mv;
		if (tt_entry->depth >= depth && !pv_node) {
			int s = tt_entry->score;
			if (tt_entry->flag == TT_EXACT) {
				if (best_move) *best_move = tt_entry->best_move;
				return s;
			}
			if (tt_entry->flag == TT_ALPHA && s <= alpha) return alpha;
			if (tt_entry->flag == TT_BETA  && s >= beta)  return beta;
		}
	}

	if (depth <= 0) return quiescence(p, alpha, beta);

	bool in_check = is_in_check(p);
	if (in_check) depth++;

	/* Null move pruning */
	if (do_null && !in_check && !pv_node && depth >= 3 && ply > 0) {
		Color us = p->white_turn ? WHITE : BLACK;
		Bitboard majors = p->pieces[us][KNIGHT] | p->pieces[us][BISHOP]
		                | p->pieces[us][ROOK]   | p->pieces[us][QUEEN];
		if (majors) {
			Position np = *p;
			np.white_turn = !np.white_turn;
			np.en_passant = -1;
			np.hash ^= zobrist_side_key();
			if (p->en_passant >= 0)
				np.hash ^= zobrist_ep_key(p->en_passant & 7);
			int R = 2 + (depth >= 6 ? 1 : 0);
			int ns = -negamax(&np, depth - 1 - R, -beta, -beta + 1,
			                  ply + 1, NULL, false);
			if (ns >= beta) return beta;
		}
	}

	MoveList moves;
	generate_legal_moves(p, &moves);
	if (moves.count == 0)
		return in_check ? -(SCORE_MATE - ply) : 0;

	int scores[MAX_MOVES];
	score_moves(p, &moves, scores, tt_move, ply);

	Move local_best = moves.moves[0];
	int searched = 0;

	for (int i = 0; i < moves.count; i++) {
		pick_best(&moves, scores, i);
		Position child = *p;
		make_move(&child, &moves.moves[i]);

		int score;
		bool tactical = MOVE_IS_CAPTURE(moves.moves[i].flags)
		             || MOVE_IS_PROMO(moves.moves[i].flags);
		bool killer = (ply < MAX_PLY)
			&& ((moves.moves[i].from == killers[ply][0].from
			     && moves.moves[i].to == killers[ply][0].to)
			 || (moves.moves[i].from == killers[ply][1].from
			     && moves.moves[i].to == killers[ply][1].to));

		if (searched == 0) {
			/* PVS: search first move with full window */
			score = -negamax(&child, depth - 1, -beta, -alpha,
			                 ply + 1, NULL, true);
		} else {
			/* LMR + PVS: late moves get reduced zero-window search */
			int reduction = 0;
			if (searched >= 4 && depth >= 3
			    && !in_check && !tactical && !killer)
				reduction = 1 + (searched >= 8 ? 1 : 0);

			score = -negamax(&child, depth - 1 - reduction,
			                 -alpha - 1, -alpha, ply + 1, NULL, true);

			/* Re-search at full depth if reduced search beats alpha */
			if (reduction > 0 && score > alpha)
				score = -negamax(&child, depth - 1, -alpha - 1, -alpha,
				                 ply + 1, NULL, true);

			/* PVS re-search with full window if zero-window beats alpha */
			if (score > alpha && score < beta)
				score = -negamax(&child, depth - 1, -beta, -alpha,
				                 ply + 1, NULL, true);
		}
		searched++;

		if (score >= beta) {
			if (!tactical) {
				store_killer(&moves.moves[i], ply);
				update_history(p, &moves.moves[i], depth);
			}
			tt_store(p->hash, beta, depth, TT_BETA, moves.moves[i]);
			if (best_move) *best_move = moves.moves[i];
			return beta;
		}
		if (score > alpha) {
			alpha = score;
			local_best = moves.moves[i];
		}
	}

	int flag = (alpha <= orig_alpha) ? TT_ALPHA : TT_EXACT;
	tt_store(p->hash, alpha, depth, flag, local_best);
	if (best_move) *best_move = local_best;
	return alpha;
}

#define ASP_WINDOW 50

int engine_search(const Position *p, int max_depth, Move *best_move) {
	Move iter_best = {0};
	int iter_score = 0;
	engine_stop = 0;
	nodes = 0;
	search_start_time = 0;
	search_time_limit = 0;
	memset(killers, 0, sizeof(killers));
	for (int c = 0; c < 2; c++)
		for (int f = 0; f < 64; f++)
			for (int t = 0; t < 64; t++)
				history[c][f][t] /= 4;
	for (int depth = 1; depth <= max_depth; depth++) {
		int alpha, beta;
		if (depth >= 4) {
			alpha = iter_score - ASP_WINDOW;
			beta  = iter_score + ASP_WINDOW;
		} else {
			alpha = -SCORE_INF;
			beta  = SCORE_INF;
		}
		int score = negamax(p, depth, alpha, beta, 0, &iter_best, true);
		if (score <= alpha || score >= beta)
			score = negamax(p, depth, -SCORE_INF, SCORE_INF,
			                0, &iter_best, true);
		iter_score = score;
		if (score > SCORE_MATE - MAX_PLY || score < -SCORE_MATE + MAX_PLY)
			break;
	}
	if (best_move) *best_move = iter_best;
	return iter_score;
}

static int extract_pv(const Position *pos, Move *pv, int max_len) {
	Position p = *pos;
	int len = 0;
	uint64_t seen[MAX_PLY];
	for (int i = 0; i < max_len && i < MAX_PLY; i++) {
		bool cycle = false;
		for (int j = 0; j < len; j++)
			if (seen[j] == p.hash) { cycle = true; break; }
		if (cycle) break;
		TTEntry *e = tt_probe(p.hash);
		if (!e) break;
		Move m = e->best_move;
		MoveList legal;
		generate_legal_moves(&p, &legal);
		bool found = false;
		for (int j = 0; j < legal.count; j++)
			if (legal.moves[j].from == m.from &&
			    legal.moves[j].to == m.to &&
			    legal.moves[j].flags == m.flags) { found = true; break; }
		if (!found) break;
		seen[len] = p.hash;
		pv[len] = m;
		len++;
		make_move(&p, &m);
	}
	return len;
}

int engine_search_uci(const Position *p, int max_depth,
                      int64_t time_limit_ms, Move *best_move) {
	Move iter_best = {0};
	int iter_score = 0;
	engine_stop = 0;
	nodes = 0;
	search_start_time = get_time_ms();
	search_time_limit = time_limit_ms;
	memset(killers, 0, sizeof(killers));
	for (int c = 0; c < 2; c++)
		for (int f = 0; f < 64; f++)
			for (int t = 0; t < 64; t++)
				history[c][f][t] /= 4;

	int limit = (max_depth > 0) ? max_depth : MAX_PLY;
	MoveList legal;
	generate_legal_moves(p, &legal);
	if (legal.count > 0) iter_best = legal.moves[0];

	for (int depth = 1; depth <= limit; depth++) {
		Move current_best = {0};
		int alpha, beta;
		if (depth >= 4) {
			alpha = iter_score - ASP_WINDOW;
			beta  = iter_score + ASP_WINDOW;
		} else {
			alpha = -SCORE_INF;
			beta  = SCORE_INF;
		}
		int score = negamax(p, depth, alpha, beta,
		                    0, &current_best, true);
		if (!engine_stop && (score <= alpha || score >= beta))
			score = negamax(p, depth, -SCORE_INF, SCORE_INF,
			                0, &current_best, true);
		if (engine_stop) break;
		iter_best = current_best;
		iter_score = score;

		int64_t elapsed = get_time_ms() - search_start_time;
		if (elapsed == 0) elapsed = 1;
		uint64_t nps = nodes * 1000 / (uint64_t)elapsed;

		if (score > SCORE_MATE - MAX_PLY) {
			int mate = (SCORE_MATE - score + 1) / 2;
			printf("info depth %d score mate %d nodes %llu time %lld nps %llu",
			       depth, mate, (unsigned long long)nodes,
			       (long long)elapsed, (unsigned long long)nps);
		} else if (score < -SCORE_MATE + MAX_PLY) {
			int mate = -(SCORE_MATE + score + 1) / 2;
			printf("info depth %d score mate %d nodes %llu time %lld nps %llu",
			       depth, mate, (unsigned long long)nodes,
			       (long long)elapsed, (unsigned long long)nps);
		} else {
			printf("info depth %d score cp %d nodes %llu time %lld nps %llu",
			       depth, score, (unsigned long long)nodes,
			       (long long)elapsed, (unsigned long long)nps);
		}

		Move pv[MAX_PLY];
		int pv_len = extract_pv(p, pv, depth);
		if (pv_len > 0) {
			printf(" pv");
			for (int i = 0; i < pv_len; i++) {
				char buf[8];
				move_to_str(&pv[i], buf);
				printf(" %s", buf);
			}
		}
		printf("\n");
		fflush(stdout);

		if (score > SCORE_MATE - MAX_PLY || score < -SCORE_MATE + MAX_PLY)
			break;
		if (time_limit_ms > 0 && elapsed >= time_limit_ms / 2)
			break;
	}
	if (best_move) *best_move = iter_best;
	return iter_score;
}
