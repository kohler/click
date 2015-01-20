## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	`@clickbindir@/click-buildtool --toolcflags`

bin_PROGRAMS = clicky

clicky_SOURCES = \
	support.c support.h \
	gathererror.cc gathererror.hh \
	permstr.cc permstr.hh \
	scopechain.cc scopechain.hh \
	crouter.cc crouter.hh \
	cdriver.cc cdriver.hh \
	hvalues.cc hvalues.hh \
	dstyle.cc dstyle.hh \
	wmain.cc wmain.hh \
	dialogs.cc \
	dwidget.cc dwidget.hh \
	ddecor.cc ddecor.hh \
	whandler.cc whandler.hh \
	wdiagram.cc wdiagram.hh \
	tmain.cc tmain.hh \
	rectangle.hh \
	rectsearch.hh \
	transform.cc transform.hh \
	ref.hh \
	clicky.cc \
	interface.c interface.h \
	callbacks.h \
	xcallbacks.c

clicky_LDADD = @PACKAGE_LIBS@ @CLICKTOOL_LIBS@ $(INTLLIBS)
