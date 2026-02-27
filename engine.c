#include "engine.h"
#include "move.h"
#include <stdlib.h>

/* ================================================================
 * Material values (centipawns)
 * ================================================================ */

#define VAL_PAWN   100
#define VAL_KNIGHT 320
#define VAL_BISHOP 330
#define VAL_ROOK   500
#define VAL_QUEEN  900
#define VAL_KING   20000

/* ================================================================
 * Piece-square tables (from White's perspective, index 0 = a1)
 *
 * These give positional bonuses/penalties for where a piece is
 * placed. Values in centipawns. For Black, we mirror vertically.
 * ================================================================ */

/* Pawns: encourage center control and advancement */
static const int pst_pawn[64] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   /* rank 1 (never here) */
	  5,  10,  10, -20, -20,  10,  10,   5,   /* rank 2 */
	  5,  -5, -10,   0,   0, -10,  -5,   5,   /* rank 3 */
	  0,   0,   0,  20,  20,   0,   0,   0,   /* rank 4 */
	  5,   5,  10,  25,  25,  10,   5,   5,   /* rank 5 */
	 10,  10,  20,  30,  30,  20,  10,  10,   /* rank 6 */
	 50,  50,  50,  50,  50,  50,  50,  50,   /* rank 7 */
	  0,   0,   0,   0,   0,   0,   0,   0    /* rank 8 (promotes) */
};

/* Knights: prefer center, avoid edges */
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

/* Bishops: prefer diagonals, avoid corners */
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

/* Rooks: prefer 7th rank and open files */
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

/* Queen: slight preference for central positions */
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

/* King: in the opening/middlegame, prefer castled positions (stay safe) */
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
 * Helpers
 * ================================================================ */

/* Mirror a square vertically (for Black piece-square lookup) */
static int mirror_sq(int sq) {
	int file = sq & 7;
	int rank = sq >> 3;
	return (7 - rank) * 8 + file;
}

/* Sum up piece-square table values for all bits in a bitboard */
static int pst_sum(Bitboard bb, const int *table) {
	int score = 0;
	while (bb) {
		int sq = __builtin_ctzll(bb);
		score += table[sq];
		bb &= bb - 1;
	}
	return score;
}

/* Same but mirror squares (for Black) */
static int pst_sum_mirror(Bitboard bb, const int *table) {
	int score = 0;
	while (bb) {
		int sq = __builtin_ctzll(bb);
		score += table[mirror_sq(sq)];
		bb &= bb - 1;
	}
	return score;
}

/* Count bits in a bitboard */
static int popcount(Bitboard bb) {
	return __builtin_popcountll(bb);
}

/* ================================================================
 * Evaluation (from White's perspective)
 * ================================================================ */

int evaluate(const Position *p) {
	int score = 0;

	/* ---- Material ---- */
	score += VAL_PAWN   * (popcount(p->wp) - popcount(p->bp));
	score += VAL_KNIGHT * (popcount(p->wn) - popcount(p->bn));
	score += VAL_BISHOP * (popcount(p->wb) - popcount(p->bb));
	score += VAL_ROOK   * (popcount(p->wr) - popcount(p->br));
	score += VAL_QUEEN  * (popcount(p->wq) - popcount(p->bq));
	/* Kings are always present, no need to count */

	/* ---- Bishop pair bonus ---- */
	if (popcount(p->wb) >= 2) score += 30;
	if (popcount(p->bb) >= 2) score -= 30;

	/* ---- Piece-square tables ---- */
	/* White (direct lookup) */
	score += pst_sum(p->wp, pst_pawn);
	score += pst_sum(p->wn, pst_knight);
	score += pst_sum(p->wb, pst_bishop);
	score += pst_sum(p->wr, pst_rook);
	score += pst_sum(p->wq, pst_queen);
	score += pst_sum(p->wk, pst_king_mg);

	/* Black (mirrored lookup) */
	score -= pst_sum_mirror(p->bp, pst_pawn);
	score -= pst_sum_mirror(p->bn, pst_knight);
	score -= pst_sum_mirror(p->bb, pst_bishop);
	score -= pst_sum_mirror(p->br, pst_rook);
	score -= pst_sum_mirror(p->bq, pst_queen);
	score -= pst_sum_mirror(p->bk, pst_king_mg);

	/* ---- Mobility (simple: count of legal moves) ---- */
	/* This is somewhat expensive but acceptable for a simple engine.
	 * We approximate by only counting the side to move's legal moves. */
	{
		MoveList moves;
		generate_legal_moves(p, &moves);
		int mobility = moves.count;

		/* Give a small bonus for mobility */
		if (p->white_turn)
			score += mobility * 3;
		else
			score -= mobility * 3;
	}

	return score;
}

/* ================================================================
 * Move ranking
 * ================================================================ */

static int cmp_scored_moves(const void *a, const void *b) {
	const ScoredMove *sa = (const ScoredMove *)a;
	const ScoredMove *sb = (const ScoredMove *)b;
	/* Sort descending (best first) */
	return sb->score - sa->score;
}

void rank_moves(const Position *p, RankedMoveList *ranked) {
	MoveList legal;
	generate_legal_moves(p, &legal);

	ranked->count = legal.count;

	/* Evaluate each move by applying it and evaluating the result */
	for (int i = 0; i < legal.count; i++) {
		Position test;
		copy_position(&test, p);
		make_move(&test, &legal.moves[i]);

		/* evaluate() returns score from White's perspective.
		 * We want score from the moving side's perspective. */
		int eval = evaluate(&test);
		if (!p->white_turn) eval = -eval;

		ranked->moves[i].move = legal.moves[i];
		ranked->moves[i].score = eval;
	}

	/* Sort best-first */
	qsort(ranked->moves, ranked->count, sizeof(ScoredMove),
	      cmp_scored_moves);
}
