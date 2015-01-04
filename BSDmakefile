.include "Makefile"
VERSION!=$(GIT) describe --tags --always --dirty --match "[0-9A-Z]*.[0-9A-Z]*"
NCURS_INC!=$(NCURS_CONF) --cflags
NCURS_INC+=-I/usr/local/include
NCURS_LIB!=$(NCURS_CONF) --libs

$(DEPS): $(.PREFIX).c
	$(CC) $(CFLAGS) $(LDFLAGS) $(NCURS_INC) -c $< -o $@
