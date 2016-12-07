# USER_CROSS_COMPILE is a string that is prepended to all toolchain
# executables, such as gcc, ld, as, objcopy etc.  This is used for
# cross-compiling userspace binaries.  If not defined, CROSS_COMPILE
# is used.
USER_CROSS_COMPILE ?= $(CROSS_COMPILE)
CC = $(USER_CROSS_COMPILE)gcc
CFLAGS = -g -O2 -W -Wall
LDFLAGS =
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
MAN8DIR = $(MANDIR)/man8
INSTALL = install

PROGRAMS = ath_info
MANS = ath_info.8

CPPFLAGS_FILE = $(shell echo "\#include <stdio.h>" | \
		  $(CC) -D_FILE_OFFSET_BITS=64 -E - >/dev/null 2>&1 && \
		  echo "-D_FILE_OFFSET_BITS=64")

all: $(PROGRAMS)

ath_info: ath_info.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS_FILE) -c $<

clean:
	rm -f *.o $(PROGRAMS)

install:
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(PROGRAMS) $(DESTDIR)$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(MAN8DIR)
	$(INSTALL) -m 644 $(MANS) $(DESTDIR)$(MAN8DIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGRAMS)
	rm -f $(DESTDIR)$(MAN8DIR)/$(MANS)

.SUFFIXES: .c .o
.PHONY: all clean install uninstall
