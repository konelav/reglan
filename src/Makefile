CC = gcc
CFLAGS = -Wall -O3
LIBS=

SOURCES = arith.c parse.c node.c alternative.c print.c reglan.c
HEADERS = reglan.h
OBJS=$(SOURCES:.c=.o)

default: release

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

release: $(OBJS)
	$(CC) $(OBJS) $(LIBS) $(CFLAGS) -o reglan

debug:
	$(CC) -DDEBUG $(LIBS) $(CFLAGS) $(SOURCES) -o reglan_dbg

.PHONY: clean

clean:
	rm -f *.o reglan reglan_dbg
