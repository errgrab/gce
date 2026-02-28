#define _POSIX_C_SOURCE 200809L
#include "board.h"
#include "attack.h"
#include "movegen.h"
#include "move.h"
#include "engine.h"
#include "uci.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void print_help(void) {
	printf("Commands:\n"
	       "  <move>   SAN (e4, Nf3, O-O) or coordinate (e2e4, e7e8q)\n"
	       "  moves    List all legal moves\n"
	       "  eval     Evaluate position\n"
	       "  top      Show top 5 engine moves\n"
	       "  go       Engine plays best move\n"
	       "  uci      Enter UCI mode\n"
	       "  check    Show if in check\n"
	       "  board    Redraw board\n"
	       "  reset    Reset to start\n"
	       "  help     Show this help\n"
	       "  quit     Exit\n\n");
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

int main(int argc, char **argv) {
	init_attacks();
	init_zobrist();
	engine_init();

	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "--uci") == 0 || strcmp(argv[i], "uci") == 0) {
			uci_loop();
			return 0;
		}

	int interactive = isatty(STDIN_FILENO);
	Position pos;
	init_position(&pos);
	char input[4096];

	if (interactive) {
		printf("=== G Chess Engine ===\n");
		printf("Type 'help' for commands.\n\n");
		print_board(&pos);
	}

	while (1) {
		GameState state = get_game_state(&pos);
		if (state != GAME_ONGOING && interactive) {
			printf("*** %s ***\n", game_state_str(state));
			if (state == GAME_CHECKMATE)
				printf("%s wins!\n", pos.white_turn ? "Black" : "White");
			printf("Type 'reset' to play again or 'quit' to exit.\n\n");
		}
		if (interactive && is_in_check(&pos) && state == GAME_ONGOING)
			printf("Check!\n");
		if (interactive) {
			printf("%s> ", pos.white_turn ? "White" : "Black");
			fflush(stdout);
		}
		if (!fgets(input, sizeof(input), stdin)) break;
		input[strcspn(input, "\n")] = '\0';
		if (input[0] == '\0') continue;

		if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) break;
		if (strcmp(input, "uci") == 0) { uci_loop(); return 0; }
		if (strcmp(input, "help") == 0 || strcmp(input, "h") == 0) {
			print_help(); continue;
		}
		if (strcmp(input, "reset") == 0) {
			init_position(&pos);
			printf("Board reset.\n\n");
			print_board(&pos);
			continue;
		}
		if (strcmp(input, "board") == 0) { print_board(&pos); continue; }
		if (strcmp(input, "moves") == 0) { print_legal_moves(&pos); continue; }
		if (strcmp(input, "check") == 0) {
			printf("%s\n\n", is_in_check(&pos) ? "In check!" : "Not in check.");
			continue;
		}
		if (strcmp(input, "eval") == 0) {
			int score = evaluate(&pos);
			int stm = pos.white_turn ? score : -score;
			printf("Eval: %+.2f (%s)  Raw: %+.2f\n\n",
				stm / 100.0,
				pos.white_turn ? "White" : "Black",
				score / 100.0);
			continue;
		}
		if (strcmp(input, "top") == 0) {
			if (state != GAME_ONGOING) {
				printf("Game is over. Type 'reset' to play again.\n");
				continue;
			}
			MoveList list;
			generate_legal_moves(&pos, &list);
			Move top_moves[256];
			int top_scores[256];
			for (int i = 0; i < list.count; i++) {
				Position copy = pos;
				make_move(&copy, &list.moves[i]);
				Move dummy;
				top_scores[i] = -engine_search(&copy, DEFAULT_DEPTH - 1, &dummy);
				top_moves[i] = list.moves[i];
			}
			for (int i = 0; i < list.count - 1; i++)
				for (int j = i + 1; j < list.count; j++)
					if (top_scores[j] > top_scores[i]) {
						int ts = top_scores[i]; top_scores[i] = top_scores[j]; top_scores[j] = ts;
						Move tm = top_moves[i]; top_moves[i] = top_moves[j]; top_moves[j] = tm;
					}
			int n = list.count < 5 ? list.count : 5;
			printf("Top %d moves:\n", n);
			for (int i = 0; i < n; i++) {
				char san[12];
				move_to_san(&top_moves[i], &pos, san);
				printf("  %d. %-8s %+.2f\n", i + 1, san, top_scores[i] / 100.0);
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
			Position before = pos;
			make_move(&pos, &best);
			char san[12];
			move_to_san(&best, &before, san);
			printf("Engine plays: %d.%s%s (eval: %+.2f)\n\n",
				before.fullmove,
				before.white_turn ? " " : ".. ",
				san, score / 100.0);
			print_board(&pos);
			continue;
		}

		/* Try to parse a move */
		if (state != GAME_ONGOING) {
			printf("Game is over. Type 'reset' to play again.\n");
			continue;
		}
		Position before = pos;
		Move played;
		const char *err = try_make_move(&pos, input, &played);
		if (err) {
			printf("Error: %s\nType 'moves' for legal moves.\n", err);
			continue;
		}
		char san[12];
		move_to_san(&played, &before, san);
		printf("  %d.%s%s\n\n",
			before.fullmove,
			before.white_turn ? " " : ".. ",
			san);
		print_board(&pos);
	}
	printf("Bye!\n");
	return 0;
}
