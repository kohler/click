## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	`@clickbindir@/click-compile --toolcflags`

bin_PROGRAMS = clicky

clicky_SOURCES = \
	interface.c interface.h \
	support.c support.h \
	gathererror.cc gathererror.hh \
	permstr.cc permstr.hh \
	wrouter.cc wrouter.hh \
	dialogs.cc \
	dwidget.cc dwidget.hh \
	dstyle.cc dstyle.hh \
	hvalues.cc hvalues.hh \
	whandler.cc whandler.hh \
	wdriver.cc wdriver.hh \
	diagram.cc diagram.hh \
	rectangle.hh \
	rectsearch.hh \
	ref.hh \
	clicky.cc \
	callbacks.h \
	xcallbacks.c

clicky_LDADD = @PACKAGE_LIBS@ @CLICKTOOL_LIBS@ $(INTLLIBS)
