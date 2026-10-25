SUBDIRS = testsuite deb

docdir = $(prefix)/doc/$(PACKAGE)-$(VERSION)

doc_DATA = AUTHORS COPYING ChangeLog INSTALL NEWS README

bin_PROGRAMS = chrpath

man_MANS = chrpath.1

debs:
	fakeroot debian/rules binary

chrpath_SOURCES = \
	chrpath.c	\
	killrpath.c	\
	main.c		\
	elf.c		\
	protos.h

EXTRA_DIST = $(man_MANS)

config-updates:
	GET 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD' > config.guess
	GET 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD' > config.sub

CLEANFILES = *.bb *.bbg *.da *.gcov testsuite/*.bb testsuite/*.bbg
coverage: all check
	for f in *.c; do gcov $$f; done
