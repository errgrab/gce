#include "engine.h"
#include "move.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Material values (centipawns)
 * ================================================================ */

#define VAL_PAWN   100
#define VAL_KNIGHT 320
#define VAL_BISHOP 330
#define VAL_ROOK   500
#define VAL_QUEEN  900
#define VAL_KING   20000

/* Piece value lookup indexed by PieceType enum (PIECE_NONE=0..KING=6) */
static const int piece_value[7] = {
	0, VAL_PAWN, VAL_KNIGHT, VAL_BISHOP, VAL_ROOK, VAL_QUEEN, VAL_KING
};

/* ================================================================
 * Piece-square tables (from White's perspective, index 0 = a1)
 * ================================================================ */

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

/* ================================================================
 * Eval helpers
 * ================================================================ */

static int mirror_sq(int sq) {
	return (7 - (sq >> 3)) * 8 + (sq & 7);
}

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
		score += table[mirror_sq(sq)];
		bb &= bb - 1;
	}
	return score;
}

static int popcount(Bitboard bb) {
	return __builtin_popcountll(bb);
}

/* ================================================================
 * Static evaluation (from White's perspective)
 * ================================================================ */

int evaluate(const Position *p) {
	int score = 0;

	score += VAL_PAWN   * (popcount(p->wp) - popcount(p->bp));
	score += VAL_KNIGHT * (popcount(p->wn) - popcount(p->bn));
	score += VAL_BISHOP * (popcount(p->wb) - popcount(p->bb));
	score += VAL_ROOK   * (popcount(p->wr) - popcount(p->br));
	score += VAL_QUEEN  * (popcount(p->wq) - popcount(p->bq));

	if (popcount(p->wb) >= 2) score += 30;
	if (popcount(p->bb) >= 2) score -= 30;

	score += pst_sum(p->wp, pst_pawn);
	score += pst_sum(p->wn, pst_knight);
	score += pst_sum(p->wb, pst_bishop);
	score += pst_sum(p->wr, pst_rook);
	score += pst_sum(p->wq, pst_queen);
	score += pst_sum(p->wk, pst_king_mg);

	score -= pst_sum_mirror(p->bp, pst_pawn);
	score -= pst_sum_mirror(p->bn, pst_knight);
	score -= pst_sum_mirror(p->bb, pst_bishop);
	score -= pst_sum_mirror(p->br, pst_rook);
	score -= pst_sum_mirror(p->bq, pst_queen);
	score -= pst_sum_mirror(p->bk, pst_king_mg);

	return score;
}

/* ================================================================
 * Transposition Table
 *
 * Stores previously searched positions to avoid redundant work.
 * Uses Zobrist hash as key with "always replace" scheme.
 * ================================================================ */

/* TT entry flags */
#define TT_EXACT  0
#define TT_ALPHA  1  /* upper bound (failed low) */
#define TT_BETA   2  /* lower bound (failed high / beta cutoff) */

typedef struct {
	uint64_t key;     /* Zobrist hash (full, for collision detection) */
	int score;        /* search score */
	int depth;        /* depth of the search that produced this entry */
	int flag;         /* TT_EXACT, TT_ALPHA, or TT_BETA */
	Move best_move;   /* best move found (for move ordering) */
} TTEntry;

/* TT size: 2^20 entries = ~40 MB (each entry ~40 bytes) */
#define TT_SIZE (1 << 20)
#define TT_MASK (TT_SIZE - 1)

static TTEntry tt[TT_SIZE];

/* ================================================================
 * Killer Moves
 *
 * Stores two non-capture moves per ply that caused beta cutoffs.
 * These are tried early in move ordering (after captures, before
 * quiet moves) because they're likely good in sibling nodes.
 * ================================================================ */

static Move killers[MAX_PLY][2];

/* ================================================================
 * History Heuristic
 *
 * Tracks how often a move (by piece color, from, to) causes a
 * beta cutoff. Used for ordering quiet moves.
 * ================================================================ */

static int history[2][64][64];  /* [color][from][to] */

/* ================================================================
 * Engine initialization
 * ================================================================ */

void engine_init(void) {
	memset(tt, 0, sizeof(tt));
	memset(killers, 0, sizeof(killers));
	memset(history, 0, sizeof(history));
}

/* ================================================================
 * TT operations
 * ================================================================ */

static TTEntry *tt_probe(uint64_t key) {
	TTEntry *e = &tt[key & TT_MASK];
	if (e->key == key) return e;
	return NULL;
}

static void tt_store(uint64_t key, int score, int depth, int flag, Move best) {
	TTEntry *e = &tt[key & TT_MASK];
	/* Replace if: empty, same position, or deeper/equal search */
	if (e->key == 0 || e->key == key || depth >= e->depth) {
		e->key = key;
		e->score = score;
		e->depth = depth;
		e->flag = flag;
		e->best_move = best;
	}
}

/* ================================================================
 * Move ordering
 *
 * Order: TT move > captures (MVV-LVA) > killers > history > quiet
 * ================================================================ */

static int score_move(const Position *p, const Move *m,
                      const Move *tt_move, int ply) {
	/* TT best move gets highest priority */
	if (tt_move && m->from == tt_move->from && m->to == tt_move->to
	    && m->flags == tt_move->flags) {
		return 100000;
	}

	/* Captures: MVV-LVA */
	if (MOVE_IS_CAPTURE(m->flags)) {
		PieceType victim;
		PieceType attacker = piece_type_at(p, m->from);
		if (m->flags == MOVE_EP_CAPTURE) {
			victim = PAWN;
		} else {
			victim = piece_type_at(p, m->to);
		}
		return 50000 + piece_value[victim] * 10 - piece_value[attacker];
	}

	/* Promotions */
	if (MOVE_IS_PROMO(m->flags)) {
		return 48000;
	}

	/* Killer moves */
	if (ply < MAX_PLY) {
		if (m->from == killers[ply][0].from && m->to == killers[ply][0].to)
			return 40000;
		if (m->from == killers[ply][1].from && m->to == killers[ply][1].to)
			return 39000;
	}

	/* History heuristic */
	int color = p->white_turn ? 0 : 1;
	return history[color][m->from][m->to];
}

static void score_moves_for_ordering(const Position *p, MoveList *list,
                                     int *scores, const Move *tt_move, int ply) {
	for (int i = 0; i < list->count; i++) {
		scores[i] = score_move(p, &list->moves[i], tt_move, ply);
	}
}

static void pick_best_move(MoveList *list, int *scores, int start) {
	int best_idx = start;
	int best_score = scores[start];
	for (int i = start + 1; i < list->count; i++) {
		if (scores[i] > best_score) {
			best_score = scores[i];
			best_idx = i;
		}
	}
	if (best_idx != start) {
		Move tmp = list->moves[start];
		list->moves[start] = list->moves[best_idx];
		list->moves[best_idx] = tmp;

		int stmp = scores[start];
		scores[start] = scores[best_idx];
		scores[best_idx] = stmp;
	}
}

/* Store a killer move (non-capture that caused beta cutoff) */
static void store_killer(const Move *m, int ply) {
	if (ply >= MAX_PLY) return;
	/* Don't store if it's the same as slot 0 */
	if (killers[ply][0].from == m->from && killers[ply][0].to == m->to)
		return;
	killers[ply][1] = killers[ply][0];
	killers[ply][0] = *m;
}

/* Update history score for a move that caused a beta cutoff */
static void update_history(const Position *p, const Move *m, int depth) {
	int color = p->white_turn ? 0 : 1;
	history[color][m->from][m->to] += depth * depth;
	/* Cap to prevent overflow */
	if (history[color][m->from][m->to] > 30000)
		history[color][m->from][m->to] = 30000;
}

/* ================================================================
 * Quiescence Search
 * ================================================================ */

static int quiescence(const Position *p, int alpha, int beta) {
	int eval = evaluate(p);
	if (!p->white_turn) eval = -eval;

	if (eval >= beta) return beta;
	if (eval > alpha) alpha = eval;

	MoveList captures;
	generate_legal_captures(p, &captures);

	int scores[MAX_MOVES];
	score_moves_for_ordering(p, &captures, scores, NULL, MAX_PLY);

	for (int i = 0; i < captures.count; i++) {
		pick_best_move(&captures, scores, i);

		/* Delta pruning: skip captures that can't possibly raise alpha.
		 * If the captured piece value + a margin can't beat alpha, skip. */
		PieceType victim = piece_type_at(p, captures.moves[i].to);
		if (captures.moves[i].flags == MOVE_EP_CAPTURE) victim = PAWN;
		if (eval + piece_value[victim] + 200 < alpha
		    && !MOVE_IS_PROMO(captures.moves[i].flags)) {
			continue;
		}

		Position child;
		copy_position(&child, p);
		make_move(&child, &captures.moves[i]);

		int score = -quiescence(&child, -beta, -alpha);

		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}

	return alpha;
}

/* ================================================================
 * Negamax search with alpha-beta + all optimizations
 * ================================================================ */

static int negamax(const Position *p, int depth, int alpha, int beta,
                   int ply, Move *best_move, bool do_null) {
	/* Check for draw by 50-move rule */
	if (p->halfmove >= 100) return 0;

	int orig_alpha = alpha;

	/* ---- Transposition Table probe ---- */
	TTEntry *tt_entry = tt_probe(p->hash);
	Move tt_move_val;
	Move *tt_move = NULL;

	if (tt_entry) {
		tt_move_val = tt_entry->best_move;
		tt_move = &tt_move_val;

		if (tt_entry->depth >= depth) {
			int tt_score = tt_entry->score;
			if (tt_entry->flag == TT_EXACT) {
				if (best_move) *best_move = tt_entry->best_move;
				return tt_score;
			}
			if (tt_entry->flag == TT_ALPHA && tt_score <= alpha) {
				return alpha;
			}
			if (tt_entry->flag == TT_BETA && tt_score >= beta) {
				return beta;
			}
		}
	}

	/* ---- Leaf node: quiescence search ---- */
	if (depth <= 0) {
		return quiescence(p, alpha, beta);
	}

	bool in_check = is_in_check(p);

	/* ---- Check extension: don't reduce search when in check ---- */
	if (in_check) depth++;

	/* ---- Null Move Pruning ----
	 * If we can "pass" and still get a score >= beta, the position is
	 * so good that we can prune this branch. Skip when in check, at
	 * low depth, or in endgame (zugzwang risk). */
	if (do_null && !in_check && depth >= 3 && ply > 0) {
		/* Simple endgame detection: skip if side has only pawns + king */
		Bitboard majors = p->white_turn
			? (p->wn | p->wb | p->wr | p->wq)
			: (p->bn | p->bb | p->br | p->bq);

		if (majors) {
			/* Make a "null move": just toggle the turn */
			Position null_pos;
			copy_position(&null_pos, p);
			null_pos.white_turn = !null_pos.white_turn;
			null_pos.en_passant = -1;
			null_pos.hash = compute_hash(&null_pos);

			int R = 2 + (depth >= 6 ? 1 : 0); /* reduction */
			int null_score = -negamax(&null_pos, depth - 1 - R,
			                          -beta, -beta + 1, ply + 1, NULL, false);

			if (null_score >= beta) {
				return beta;
			}
		}
	}

	/* ---- Generate and search moves ---- */
	MoveList moves;
	generate_legal_moves(p, &moves);

	if (moves.count == 0) {
		if (in_check) return -(SCORE_MATE - ply);
		return 0; /* stalemate */
	}

	int scores[MAX_MOVES];
	score_moves_for_ordering(p, &moves, scores, tt_move, ply);

	Move local_best = moves.moves[0];
	int moves_searched = 0;

	for (int i = 0; i < moves.count; i++) {
		pick_best_move(&moves, scores, i);

		Position child;
		copy_position(&child, p);
		make_move(&child, &moves.moves[i]);

		int score;

		/* ---- Late Move Reductions (LMR) ----
		 * After searching a few moves fully, search the rest with
		 * reduced depth. If they beat alpha, re-search at full depth.
		 * Don't reduce: captures, promotions, killers, when in check. */
		bool is_tactical = MOVE_IS_CAPTURE(moves.moves[i].flags)
		                || MOVE_IS_PROMO(moves.moves[i].flags);
		bool is_killer = (ply < MAX_PLY)
		    && ((moves.moves[i].from == killers[ply][0].from
		         && moves.moves[i].to == killers[ply][0].to)
		     || (moves.moves[i].from == killers[ply][1].from
		         && moves.moves[i].to == killers[ply][1].to));

		if (moves_searched >= 4 && depth >= 3
		    && !in_check && !is_tactical && !is_killer) {
			/* Reduced search */
			int R = 1 + (moves_searched >= 8 ? 1 : 0);
			score = -negamax(&child, depth - 1 - R, -beta, -alpha,
			                 ply + 1, NULL, true);

			/* If it improved alpha, re-search at full depth */
			if (score > alpha) {
				score = -negamax(&child, depth - 1, -beta, -alpha,
				                 ply + 1, NULL, true);
			}
		} else {
			score = -negamax(&child, depth - 1, -beta, -alpha,
			                 ply + 1, NULL, true);
		}

		moves_searched++;

		if (score >= beta) {
			/* Beta cutoff */
			if (!is_tactical) {
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

	/* ---- Store in TT ---- */
	int flag = (alpha <= orig_alpha) ? TT_ALPHA : TT_EXACT;
	tt_store(p->hash, alpha, depth, flag, local_best);

	if (best_move) *best_move = local_best;
	return alpha;
}

/* ================================================================
 * Iterative Deepening
 *
 * Search depth 1, then 2, then 3... up to max_depth.
 * Each iteration's best move is used to order the next iteration,
 * dramatically improving alpha-beta efficiency.
 * ================================================================ */

int engine_search(const Position *p, int max_depth, Move *best_move) {
	Move iter_best = {0};
	int iter_score = 0;

	/* Clear killers and history for fresh search */
	memset(killers, 0, sizeof(killers));
	/* Decay history instead of clearing (keeps some useful info) */
	for (int c = 0; c < 2; c++)
		for (int f = 0; f < 64; f++)
			for (int t = 0; t < 64; t++)
				history[c][f][t] /= 4;

	for (int depth = 1; depth <= max_depth; depth++) {
		int score = negamax(p, depth, -SCORE_INF, SCORE_INF,
		                    0, &iter_best, true);
		iter_score = score;

		/* If we found a forced mate, no need to search deeper */
		if (score > SCORE_MATE - MAX_PLY || score < -SCORE_MATE + MAX_PLY)
			break;
	}

	if (best_move) *best_move = iter_best;
	return iter_score;
}

/* ================================================================
 * Move ranking (public interface)
 * ================================================================ */

static int cmp_scored_moves(const void *a, const void *b) {
	const ScoredMove *sa = (const ScoredMove *)a;
	const ScoredMove *sb = (const ScoredMove *)b;
	return sb->score - sa->score;
}

void rank_moves(const Position *p, RankedMoveList *ranked) {
	MoveList legal;
	generate_legal_moves(p, &legal);

	ranked->count = legal.count;

	for (int i = 0; i < legal.count; i++) {
		Position child;
		copy_position(&child, p);
		make_move(&child, &legal.moves[i]);

		int score = -engine_search(&child, DEFAULT_DEPTH - 1, NULL);

		ranked->moves[i].move = legal.moves[i];
		ranked->moves[i].score = score;
	}

	qsort(ranked->moves, ranked->count, sizeof(ScoredMove),
	      cmp_scored_moves);
}
