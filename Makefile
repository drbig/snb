CC?=cc
CFLAGS?=-O2
CFLAGS+=-std=c99
CFLAGS+=-Wall -Werror -Wno-implicit-function-declaration
CFLAGS+=-fstack-protector-all -fsanitize-undefined-trap-on-error -fsanitize=bounds -ftrapv
CFLAGS+=-fPIC -fPIE
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

.PHONY: clean check style docs analyze full-check

all: version $(BINDIR)/$(PRG)

version:
	@echo "#define VERSION L\"$(VERSION)\"" > $(SRCDIR)/version.h

clean:
	@rm -f $(OBJDIR)/*.o $(BINDIR)/$(PRG) $(TESTDIR)/$(TESTS)

debug: CFLAGS+=-DDEBUG -g
debug: all

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
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -o $@ $(TESTDIR)/$(TESTS).c $(DEPS) $(NCURS_LIB) -lcheck

$(BINDIR)/$(PRG): $(DEPS)
	@mkdir -p $(BINDIR)/
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -o $@ $(SRCDIR)/$(PRG).c $(DEPS) $(NCURS_LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -c $< -o $@
