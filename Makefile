CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
OBJ = main.o board.o attack.o movegen.o move.o engine.o uci.o

gce: $(OBJ)
	$(CC) -o $@ $(OBJ)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	rm -f gce $(OBJ)
.PHONY: clean
