#include <stdio.h>
#include <string.h>
#include "board.h"
#include "attack.h"
#include "movegen.h"
#include "move.h"
#include "engine.h"

static void print_help(void) {
	printf("Commands:\n");
	printf("  <move>   Move in SAN (e.g. e4, Nf3, O-O, exd5, Rae1, e8=Q)\n");
	printf("           or coordinate notation (e.g. e2e4, e7e8q)\n");
	printf("  moves    List all legal moves (in SAN)\n");
	printf("  eval     Evaluate the current position\n");
	printf("  suggest  Show ranked list of best moves\n");
	printf("  go       Engine plays the best move for current side\n");
	printf("  check    Show if current side is in check\n");
	printf("  board    Redraw the board\n");
	printf("  reset    Reset to starting position\n");
	printf("  help     Show this help\n");
	printf("  quit     Exit\n\n");
}

static void print_legal_moves(const Position *p) {
	MoveList list;
	generate_legal_moves(p, &list);

	printf("Legal moves (%d):\n", list.count);
	for (int i = 0; i < list.count; i++) {
		char buf[12];
		move_to_san(&list.moves[i], p, buf);
		printf("  %-8s", buf);
		if ((i + 1) % 8 == 0) printf("\n");
	}
	if (list.count % 8 != 0) printf("\n");
	printf("\n");
}

int main(void) {
	/* Initialize lookup tables (must be called once) */
	init_attacks();
	init_zobrist();
	engine_init();

	Position pos;
	init_position(&pos);

	char input[64];

	printf("=== G Chess Engine ===\n");
	printf("Type 'help' for commands.\n\n");

	print_board(&pos);

	while (1) {
		/* Check game state before each prompt */
		GameState state = get_game_state(&pos);
		if (state != GAME_ONGOING) {
			printf("*** %s ***\n", game_state_str(state));
			if (state == GAME_CHECKMATE) {
				printf("%s wins!\n",
					pos.white_turn ? "Black" : "White");
			}
			printf("Type 'reset' to play again or 'quit' to exit.\n\n");
		}

		if (is_in_check(&pos) && state == GAME_ONGOING) {
			printf("Check!\n");
		}

		printf("%s> ", pos.white_turn ? "White" : "Black");
		fflush(stdout);

		if (!fgets(input, sizeof(input), stdin))
			break;

		/* Remove trailing newline */
		input[strcspn(input, "\n")] = '\0';

		if (input[0] == '\0')
			continue;

		/* ---- Commands ---- */
		if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0)
			break;

		if (strcmp(input, "help") == 0 || strcmp(input, "h") == 0) {
			print_help();
			continue;
		}

		if (strcmp(input, "reset") == 0) {
			init_position(&pos);
			printf("Board reset.\n\n");
			print_board(&pos);
			continue;
		}

		if (strcmp(input, "board") == 0) {
			print_board(&pos);
			continue;
		}

		if (strcmp(input, "moves") == 0) {
			print_legal_moves(&pos);
			continue;
		}

		if (strcmp(input, "check") == 0) {
			printf("%s\n\n", is_in_check(&pos) ? "In check!" : "Not in check.");
			continue;
		}

		if (strcmp(input, "eval") == 0) {
			int score = evaluate(&pos);
			/* Show from side-to-move perspective */
			int stm_score = pos.white_turn ? score : -score;
			printf("Evaluation: %+.2f (%s's perspective)\n",
				stm_score / 100.0,
				pos.white_turn ? "White" : "Black");
			printf("Raw (White perspective): %+.2f\n\n",
				score / 100.0);
			continue;
		}

		if (strcmp(input, "suggest") == 0) {
			RankedMoveList ranked;
			rank_moves(&pos, &ranked);

			printf("Move ranking (%d moves, depth %d):\n",
				ranked.count, DEFAULT_DEPTH);
			printf("  %-4s  %-10s  %s\n", "#", "Move", "Score");
			printf("  %-4s  %-10s  %s\n", "---", "----------", "------");
			for (int i = 0; i < ranked.count; i++) {
				char san[12];
				move_to_san(&ranked.moves[i].move, &pos, san);
				printf("  %-4d  %-10s  %+.2f\n",
					i + 1, san, ranked.moves[i].score / 100.0);
			}
			printf("\n");
			continue;
		}

		if (strcmp(input, "go") == 0) {
			if (state != GAME_ONGOING) {
				printf("Game is over. Type 'reset' to play again.\n");
				continue;
			}

			Move best;
			int score = engine_search(&pos, DEFAULT_DEPTH, &best);

			/* Save position for SAN display */
			Position before;
			copy_position(&before, &pos);

			/* Apply the best move */
			make_move(&pos, &best);

			char san_buf[12];
			move_to_san(&best, &before, san_buf);
			printf("Engine plays: %d.%s%s (eval: %+.2f)\n",
				before.fullmove,
				before.white_turn ? " " : ".. ",
				san_buf,
				score / 100.0);

			printf("\n");
			print_board(&pos);
			continue;
		}

		/* ---- Try to parse and execute a move ---- */
		if (state != GAME_ONGOING) {
			printf("Game is over. Type 'reset' to play again.\n");
			continue;
		}

		/* Save position before move for SAN display */
		Position before;
		copy_position(&before, &pos);

		Move played;
		const char *err = try_make_move(&pos, input, &played);
		if (err) {
			printf("Error: %s\n", err);
			printf("Type 'moves' to see legal moves, or 'help' for commands.\n");
			continue;
		}

		/* Show the move in SAN (computed from the pre-move position) */
		char san_buf[12];
		move_to_san(&played, &before, san_buf);
		printf("  %d.%s%s\n",
			before.fullmove,
			before.white_turn ? " " : ".. ",
			san_buf);

		printf("\n");
		print_board(&pos);
	}

	printf("Bye!\n");
	return 0;
}
