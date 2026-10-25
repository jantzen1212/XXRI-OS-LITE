CHRPATH = ../chrpath

# This test fail on some archs.  Not sure how to write it in a portable
# way. [pere 2004-09-18]
RPATH = $(LDRPATH)/usr/local/lib

noinst_PROGRAMS = prog

prog_SOURCES = \
	prog.c

EXTRA_DIST = runtest.sh

prog: prog.c
	$(CC) $(RPATH) -o $@ $<

check: $(CHRPATH)
	$(srcdir)/runtest.sh
