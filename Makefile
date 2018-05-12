CC?=cc
CFLAGS?=-O2
CFLAGS+=-std=c99
CFLAGS+=-Wall -Werror -Wno-implicit-function-declaration
CFLAGS+=-fstack-protector-all -fPIC -fPIE
LDFLAGS=
STYLE=-nA2s2SHxC100xj
BINDIR=bin
SRCDIR=src
OBJDIR=src
TESTDIR=tests
PRG=snb
DEPS=$(OBJDIR)/data.o $(OBJDIR)/ui.o $(OBJDIR)/colors.o
TESTS=check_data
GIT?=git
VERSION?=$(shell ${GIT} describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*")
NCURS_CONF?=ncursesw5-config
NCURS_INC?=$(shell ${NCURS_CONF} --cflags)
NCURS_LIB?=$(shell ${NCURS_CONF} --libs)
CHECK_INC?=$(shell pkg-config --cflags check)
CHECK_LIB?=$(shell pkg-config --libs check)
INSTALL=install
PREFIX=/usr

.PHONY: clean check style docs analyze full-check install

all: version $(BINDIR)/$(PRG)

version:
	@echo "#define VERSION L\"$(VERSION)\"" > $(SRCDIR)/version.h

clean:
	@rm -f $(OBJDIR)/*.o $(BINDIR)/$(PRG) $(TESTDIR)/$(TESTS)

debug: CFLAGS+=-DDEBUG -g
debug: all

install: all
	$(INSTALL) -Dm755 $(BINDIR)/$(PRG) $(DESTDIR)$(PREFIX)/bin/$(PRG)
	$(INSTALL) -Dm644 help.md $(DESTDIR)$(PREFIX)/share/docs/$(PRG)/help.md
	$(INSTALL) -Dm444 snb.1 $(DESTDIR)$(PREFIX)/local/man/man1/snb.1

check: $(TESTDIR)/$(TESTS)
	@echo ====== TESTING
	@./$(TESTDIR)/$(TESTS)

style:
	@echo ====== STYLING CODE
	@astyle $(STYLE) $(SRCDIR)/*.c $(SRCDIR)/*.h $(TESTDIR)/*.c

docs:
	@echo ====== GENERATING DOCS
	@mkdir -p docs/
	doxygen doxygen.conf

analyze:
	@echo ====== ANALYZING
	@scan-build --status-bugs make BINDIR=/tmp

full-check: clean style debug check analyze

tests/$(TESTS): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) ${CHECK_INC} -o $@ $(TESTDIR)/$(TESTS).c $(DEPS) $(NCURS_LIB) ${CHECK_LIB}

$(BINDIR)/$(PRG): $(DEPS)
	@mkdir -p $(BINDIR)/
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -o $@ $(SRCDIR)/$(PRG).c $(DEPS) $(NCURS_LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -c $< -o $@
