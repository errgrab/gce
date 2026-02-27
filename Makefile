CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = gce
SRCS = main.c board.c attack.c movegen.c move.c engine.c
OBJS = $(SRCS:.c=.o)
HDRS = board.h attack.h movegen.h move.h engine.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
