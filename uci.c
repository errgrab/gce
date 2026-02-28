#define _POSIX_C_SOURCE 200809L
#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "move.h"
#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

static Position pos;
static volatile int uci_quit_requested = 0;

static int stdin_has_data(void) {
	fd_set fds;
	struct timeval tv = {0, 0};
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static void uci_check_input(void) {
	if (!stdin_has_data()) return;
	char buf[256];
	if (fgets(buf, sizeof(buf), stdin)) {
		buf[strcspn(buf, "\n")] = '\0';
		buf[strcspn(buf, "\r")] = '\0';
		if (strcmp(buf, "stop") == 0)
			engine_stop = 1;
		else if (strcmp(buf, "quit") == 0) {
			engine_stop = 1;
			uci_quit_requested = 1;
		} else if (strcmp(buf, "isready") == 0) {
			printf("readyok\n");
			fflush(stdout);
		}
	}
}

static void handle_position(char *line) {
	char *ptr = line + 8;
	while (*ptr == ' ') ptr++;

	if (strncmp(ptr, "startpos", 8) == 0) {
		init_position(&pos);
		ptr += 8;
	} else if (strncmp(ptr, "fen", 3) == 0) {
		ptr += 3;
		while (*ptr == ' ') ptr++;
		char *mp = strstr(ptr, " moves ");
		if (mp) {
			*mp = '\0';
			if (!position_from_fen(&pos, ptr)) return;
			*mp = ' ';
			ptr = mp;
		} else {
			if (!position_from_fen(&pos, ptr)) return;
			return;
		}
	} else {
		return;
	}

	char *moves = strstr(ptr, "moves");
	if (!moves) return;
	moves += 5;
	while (*moves) {
		while (*moves == ' ') moves++;
		if (*moves == '\0') break;
		char ms[12];
		int i = 0;
		while (*moves && *moves != ' ' && i < 11)
			ms[i++] = *moves++;
		ms[i] = '\0';
		Move m;
		if (parse_move(ms, &pos, &m))
			make_move(&pos, &m);
		else
			break;
	}
}

static int parse_int_after(const char *str, const char *key) {
	const char *p = strstr(str, key);
	if (!p) return -1;
	p += strlen(key);
	while (*p == ' ') p++;
	if (*p == '\0' || (*p != '-' && (*p < '0' || *p > '9'))) return -1;
	return atoi(p);
}

static void handle_go(const char *line) {
	int max_depth = 0;
	int64_t time_limit = 0;

	int dv = parse_int_after(line, "depth ");
	int mt = parse_int_after(line, "movetime ");
	int wt = parse_int_after(line, "wtime ");
	int bt = parse_int_after(line, "btime ");
	int wi = parse_int_after(line, "winc ");
	int bi = parse_int_after(line, "binc ");
	int mtg = parse_int_after(line, "movestogo ");
	int inf = (strstr(line, "infinite") != NULL);

	if (dv > 0) max_depth = dv;

	if (mt > 0) {
		time_limit = mt;
	} else if (wt >= 0 || bt >= 0) {
		int our_time = pos.white_turn ? wt : bt;
		int our_inc  = pos.white_turn ? (wi > 0 ? wi : 0) : (bi > 0 ? bi : 0);
		if (our_time > 0) {
			if (mtg > 0) {
				time_limit = our_time / (mtg + 2) + our_inc;
			} else {
				int moves_est = 30;
				time_limit = our_time / moves_est + our_inc * 3 / 4;
			}
			if (time_limit > our_time / 3)
				time_limit = our_time / 3;
			if (time_limit < 50 && our_time > 200)
				time_limit = 50;
			if (time_limit < 10)
				time_limit = 10;
		}
	}

	if (max_depth == 0) {
		if (inf || time_limit > 0)
			max_depth = MAX_PLY;
		else
			max_depth = DEFAULT_DEPTH;
	}

	MoveList legal;
	generate_legal_moves(&pos, &legal);
	if (legal.count == 0) {
		printf("bestmove 0000\n");
		fflush(stdout);
		return;
	}

	engine_check_fn = uci_check_input;
	Move best;
	engine_search_uci(&pos, max_depth, time_limit, &best);
	engine_check_fn = NULL;

	char buf[8];
	move_to_str(&best, buf);
	printf("bestmove %s\n", buf);
	fflush(stdout);
}

void uci_loop(void) {
	printf("id name GCE\n");
	printf("id author GCE Team\n");
	printf("uciok\n");
	fflush(stdout);

	init_position(&pos);
	uci_quit_requested = 0;

	char line[16384];
	while (fgets(line, sizeof(line), stdin)) {
		line[strcspn(line, "\n")] = '\0';
		line[strcspn(line, "\r")] = '\0';
		if (line[0] == '\0') continue;

		if (strcmp(line, "uci") == 0) {
			printf("id name GCE\n");
			printf("id author GCE Team\n");
			printf("uciok\n");
			fflush(stdout);
		} else if (strcmp(line, "isready") == 0) {
			printf("readyok\n");
			fflush(stdout);
		} else if (strcmp(line, "ucinewgame") == 0) {
			engine_init();
			init_position(&pos);
		} else if (strncmp(line, "position", 8) == 0 &&
		           (line[8] == '\0' || line[8] == ' ')) {
			handle_position(line);
		} else if (strncmp(line, "go", 2) == 0 &&
		           (line[2] == '\0' || line[2] == ' ')) {
			handle_go(line);
			if (uci_quit_requested) break;
		} else if (strcmp(line, "stop") == 0) {
			engine_stop = 1;
		} else if (strcmp(line, "quit") == 0) {
			break;
		}
	}
}
