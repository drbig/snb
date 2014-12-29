CC=gcc
CFLAGS=-Wall -Werror
LDFLAGS=
STYLE=-nA2s2SHxC100xj
BINDIR=bin
PRG=snb
DEPS=src/data.o src/ui.o src/colors.o
TESTS=check_data
VERSION=$(shell git describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*")
NCURS_INC=$(shell ncursesw5-config --cflags)
NCURS_LIB=$(shell ncursesw5-config --libs)

.PHONY: clean check style docs analyze full-check

all: version $(BINDIR)/$(PRG)

version:
	@echo "#define VERSION L\"$(VERSION)\"" > src/version.h

clean:
	@rm -f src/*.o $(BINDIR)/$(PRG) tests/$(TESTS)

debug: CFLAGS+=-DDEBUG -g
debug: all

check: tests/$(TESTS)
	@echo ====== TESTING
	@./tests/$(TESTS)

style:
	@echo ====== STYLING CODE
	@astyle $(STYLE) src/*.c src/*.h tests/*.c

docs:
	@echo ====== GENERATING DOCS
	@mkdir -p docs/
	doxygen doxygen.conf

analyze:
	@echo ====== ANALYZING
	@scan-build --status-bugs make BINDIR=/tmp

full-check: clean style debug check analyze

tests/$(TESTS): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -o $@ tests/$(TESTS).c $(DEPS) $(NCURS_LIB) -lcheck

$(BINDIR)/$(PRG): $(DEPS)
	@mkdir -p $(BINDIR)/
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -o $@ src/$(PRG).c $(DEPS) $(NCURS_LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -c $< -o $@
