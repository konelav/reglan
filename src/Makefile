CC = gcc
AR = ar
CFLAGS = -Wall -O3
LIBS=

SOURCES = arith.c parse.c alteration.c concatenation.c print.c
HEADERS = reglan.h
OBJS=$(SOURCES:.c=.o)
OBJS_PIC=$(SOURCES:.c=.pic.o)

default: release

all: release static shared

%.pic.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) -fPIC


%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

release: $(OBJS) reglan.o
	$(CC) $(OBJS) reglan.o $(LIBS) $(CFLAGS) -o reglan

debug:
	$(CC) -DDEBUG $(LIBS) $(CFLAGS) $(SOURCES) -o reglan_dbg

static: $(OBJS)
	$(AR) -rc libreglan.a $(OBJS)

shared: $(OBJS_PIC)
	$(CC) $(OBJS_PIC) $(LIBS) $(CFLAGS) -shared -o libreglan.so

.PHONY: clean

clean:
	rm -f *.o *.a *.so reglan reglan_dbg
