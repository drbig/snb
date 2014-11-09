CC=gcc
CFLAGS=-Wall
LDFLAGS=-lncursesw
PRG=snb
DEPS=data.o ui.o colors.o
VERSION=$$(git describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*")

.PHONY: all clean debug

version:
	@echo "#define VERSION L\"$(VERSION)\"" > version.h

all: version $(PRG)

clean:
	rm -f *.o $(PRG)

debug: CFLAGS+=-DDEBUG -g
debug: all

$(PRG): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PRG).c $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $<
