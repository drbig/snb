CC=gcc
CFLAGS=-Wall -Werror
LDFLAGS=-lncursesw
STYLE=-nA2s2SHxC100xj
PRG=snb
DEPS=src/data.o src/ui.o src/colors.o
TESTS=check_data
VERSION=$$(git describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*")

.PHONY: clean check style

all: version bin/$(PRG)

version:
	@echo "#define VERSION L\"$(VERSION)\"" > src/version.h

clean:
	rm -f src/*.o bin/$(PRG) tests/$(TESTS)

debug: CFLAGS+=-DDEBUG -g
debug: all

check: tests/$(TESTS)
	@./tests/$(TESTS)

style:
	astyle $(STYLE) src/*.c src/*.h tests/*.c

tests/$(TESTS): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -lcheck -o $@ tests/$(TESTS).c $(DEPS)

bin/$(PRG): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ src/$(PRG).c $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@
