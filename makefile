#!/usr/bin/make
# make iso 13818-1 stream multiplexer

VERSION = 1.1.7
DATE = $(shell date +%Y-%m-%d)

PREFIX ?= /usr/local
INCLUDEDIR = $(PREFIX)/include
BINDIR = $(PREFIX)/bin
MAN1DIR = $(PREFIX)/share/man/man1

CFLAGS = -O -c -Wall -I$(INCLUDEDIR)
CC = gcc

OBJS_G = dispatch.o init.o error.o crc32.o input.o output.o command.o \
	global.o descref.o splitpes.o splitps.o splitts.o splice.o
OBJ_ts = splicets.o
OBJ_ps = spliceps.o
OBJS_S = $(OBJ_ts) $(OBJ_ps)
OBJS = $(OBJS_G) $(OBJS_S)
OBJS_O = repeatts.o showts.o
OBJS_TS2PES = ts2pes.o ts2pesdescr.o
OBJS_PES2ES = pes2es.o crc16.o
OBJS_EN300468TS = en300468ts.o
ALLOBJS = $(OBJS) $(OBJS_O) $(OBJS_TS2PES) $(OBJS_PES2ES) $(OBJS_EN300468TS)

TRGSTEM = iso13818
TARGETS_I = $(TRGSTEM)ts $(TRGSTEM)ps
TARGETS_O = repeatts showts
TARGET_TS2PES = ts2pes
TARGET_PES2ES = pes2es
TARGET_EN300468TS = en300468ts
TARGETS = $(TARGETS_O) $(TARGETS_I) $(TARGET_TS2PES) $(TARGET_PES2ES) \
	$(TARGET_EN300468TS)

HEADERS = dispatch.h error.h crc32.h input.h output.h command.h global.h \
	descref.h splitpes.h splitps.h splitts.h splice.h pes.h ps.h ts.h \
	makefile
DEFS_INCSRC = en300468ts.table en300468ts.descr
DEFS_MANOBJ = $(addsuffix .o,$(DEFS_INCSRC))
DEFS_INCDEF = $(addsuffix .h,$(DEFS_INCSRC))
MAN1 = $(addsuffix .1,$(TARGETS_I) repeatts)
MANGEN = $(addsuffix .1,$(TARGET_EN300468TS))
MANSRC = $(addsuffix .src,$(MANGEN))
MAN = $(MAN1) $(MANGEN)
LICENCE = COPYING
SOURCES = $(addsuffix .c,$(basename $(ALLOBJS))) \
	$(addsuffix .h,$(basename $(OBJS_TS2PES) $(OBJS_PES2ES) $(OBJS_S)))
ALLSRC = $(HEADERS) $(SOURCES) $(LICENCE) $(DEFS_INCSRC) $(DEFS_INCDEF)

.PHONY:	all clean install install_bin install_man uninstall targz

all:	$(TARGETS) $(MANGEN)

# kill stupid implicit rule:
%:      %.o

$(TARGETS_I):	$(OBJS)
	$(CC) -o $@ $(OBJS_G) $($(patsubst $(TRGSTEM)%,OBJ_%,$@))

$(TARGETS_O): % : %.o
	$(CC) -o $* $@.o

$(TARGET_TS2PES):	$(OBJS_TS2PES) crc32.o
	$(CC) -o $@ $(OBJS_TS2PES) crc32.o

$(TARGET_PES2ES):	$(OBJS_PES2ES)
	$(CC) -o $@ $(OBJS_PES2ES)

$(TARGET_EN300468TS):	$(OBJS_EN300468TS) crc32.o
	$(CC) -o $@ $(OBJS_EN300468TS) crc32.o

$(OBJS_G) $(OBJS_O):	%.o:	%.c $(HEADERS)
	$(CC) $(CFLAGS) -DMPLEX_VERSION=\"$(VERSION)\" -o $@ $<

$(OBJS_S):	%.o:	%.c %.h $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

$(OBJS_TS2PES):	%.o:	%.c %.h
	$(CC) $(CFLAGS) -o $@ $<

$(OBJS_PES2ES):	%.o:	%.c %.h
	$(CC) $(CFLAGS) -o $@ $<

$(OBJS_EN300468TS):	%.o:	%.c $(DEFS_INCSRC) makefile
	$(CC) $(CFLAGS) -o $@ $<

install:	install_bin install_man

install_bin:	$(TARGETS)
	install -d $(BINDIR)
	install -c -m 755 $(TARGETS) $(BINDIR)

install_man:	$(MAN)
	install -d $(MAN1DIR)
	install -c -m 644 $(MAN1) $(MANGEN) $(MAN1DIR)

uninstall:
	cd $(BINDIR) ; rm -vf $(TARGETS)
	cd $(MAN1DIR) ; rm -vf $(MAN1) $(MANGEN)

clean:
	rm -f *.o *~ $(TARGETS) $(MANGEN)

$(DEFS_MANOBJ):	%.o:	% %.h
	$(CC) -E -o - -x c -include $<.h $< | grep -v '^#' | grep -v '^$$' >$@

en300468ts.1:   %.1:    %.1.src $(DEFS_MANOBJ)
	sed -e '/^\.\\" INCLUDE-TABLE$$/r en300468ts.table.o' \
	  -e '/^\.\\" INCLUDE-DESCR$$/r en300468ts.descr.o' <$< >$@

targz:	$(ALLSRC) $(MAN1) $(MANSRC)
	mkdir mplex13818-$(VERSION)
	ln $(ALLSRC) mplex13818-$(VERSION)/.
	for i in $(MAN1) $(MANSRC) ; do \
	  sed -e 's/^\(.TH.*\)"DATE" "VERSION"/\1"$(DATE)" "$(VERSION)"/' \
	    <$$i >mplex13818-$(VERSION)/$$i ; done
	tar -czf mplex13818-$(VERSION).tar.gz mplex13818-$(VERSION)
	rm -r mplex13818-$(VERSION)
