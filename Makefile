CC=gcc
CFLAGS=-Wall
LDFLAGS=-lncursesw
PRG=snb
DEPS=data.o ui.o colors.o
TESTS=data dupa

.PHONY: all clean debug

all: $(PRG)

clean:
	rm -f *.o $(PRG)

debug: CFLAGS+=-DDEBUG -g
debug: all

$(PRG): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PRG).c $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $<
