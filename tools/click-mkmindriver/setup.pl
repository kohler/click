#!/usr/bin/perl

if ($#ARGV != 2) {
  print "Usage: setup.pl click_dir build_dir conf_file\n";
  print "click_dir: top level click directory.\n";
  print "build_dir: directory to setup build files, must be relative path.\n";
  print "           build directory will be created in click_dir/build_dir.\n";
  print "conf_file: name of the configuration file to parse.\n";
  print "\n";
  print "For example: setup.pl ~/click tiny ~/click/conf/test.conf\n\n";
  print "   will setup a build environment in ~/click/tiny. The\n";
  print "   executable produced will be click. ~/click/conf/test.conf\n";
  print "   will be used as the router file.\n";
  print "\n";
  exit;
}

$click_dir = $ARGV[0];
$build_dir = $ARGV[1];
$conf_file = $ARGV[2];

$dir = "$click_dir/$build_dir";

$r = system("mkdir -p $dir");
if ($r != 0) {
  print "cannot create $dir, exiting...\n";
  exit;
}

$makefile = "$dir/Makefile.in";
open(F0, "> $makefile");
print F0 <<EOF;

SHELL = \@SHELL\@

srcdir := \@srcdir\@
top_srcdir := \@top_srcdir\@
top_builddir := ..
subdir := $build_dir

prefix = \@prefix\@
exec_prefix = \@exec_prefix\@
bindir = \@bindir\@
sbindir = \@sbindir\@
libdir = \@libdir\@

VPATH = .:\$(top_srcdir)/lib:\$(top_srcdir)/\$(subdir):\$(top_srcdir)/elements/userlevel\@elements_vpath\@

CC = \@CC\@
CPP = \@CPP\@
CXX = \@CXX\@
CXXCPP = \@CXXCPP\@
AR = ar
RANLIB = \@RANLIB\@
INSTALL = \@INSTALL\@
mkinstalldirs = \@top_srcdir\@/mkinstalldirs

.SUFFIXES:
.SUFFIXES: .S .c .cc .o .s .ii

.c.o:
	\$(COMPILE) -c \$<
.s.o:
	\$(COMPILE) -c \$<
.S.o:
	\$(COMPILE) -c \$<
.cc.o:
	\$(CXXCOMPILE) -c \$<
.cc.ii:
	\$(CXXCOMPILE) -E \$< > \$\@

-include elements.mk

OBJS = \$(ELEMENT_OBJS) elements.o \$(top_srcdir)/userlevel/click.o \\
	\$(top_srcdir)/userlevel/quitwatcher.o \\
	\$(top_srcdir)/userlevel/controlsocket.o

CPPFLAGS = \@CPPFLAGS\@ -MD -DCLICK_USERLEVEL
CFLAGS = \@CFLAGS\@
CXXFLAGS = \@CXXFLAGS\@

DEFS = \@DEFS\@ -DCLICK_BINDIR='"\$(bindir)"' -DCLICK_LIBDIR='"\$(libdir)"'
INCLUDES = -I\$(top_builddir) -I\$(top_builddir)/include -I\$(srcdir) \\
	-I\$(top_srcdir) -I\$(top_srcdir)/include \@PCAP_HEADER_PATH\@
LDFLAGS = \@LDFLAGS\@ \@PCAP_LIBRARY_PATH\@
LIBS = \@LIBS\@ \@PCAP_LIBRARY\@ \@DL_LIBRARY\@ \@SOCKET_LIBS\@

CXXCOMPILE = \$(CXX) \$(DEFS) \$(INCLUDES) \$(CPPFLAGS) \$(CXXFLAGS)
CXXLD = \$(CXX)
CXXLINK = \$(CXXLD) \$(CXXFLAGS) \$(LDFLAGS) -o \$\@
COMPILE = \$(CC) \$(DEFS) \$(INCLUDES) \$(CPPFLAGS) \$(CFLAGS)
CCLD = \$(CC)
LINK = \$(CCLD) \$(CFLAGS) \$(LDFLAGS) -o \$\@

all: click

click: Makefile \$(top_srcdir)/userlevel/libclick.a \$(OBJS)
	\$(CXXLINK) -rdynamic \$(OBJS) \$(LIBS) \$(top_srcdir)/userlevel/libclick.a

Makefile: \$(srcdir)/Makefile.in \$(top_builddir)/config.status
	cd \$(top_builddir) \\
	  && CONFIG_FILES=\$(subdir)/\$\@ CONFIG_HEADERS= \$(SHELL) ./config.status

elemlist:
	\@/bin/rm -f elements.conf
	\@\$(MAKE) elements.conf
elements.conf: \$(top_builddir)/config.status \$(top_srcdir)/click-buildtool
	(\$(top_srcdir)/tools/click-shrink/click-shrink $conf_file) > elements.conf
elements.mk: elements.conf \$(top_srcdir)/click-buildtool
	(cd \$(top_srcdir); ./click-buildtool elem2make -x addressinfo.o -x alignmentinfo.o -x errorelement.o -x scheduleinfo.o) < elements.conf > elements.mk
elements.cc: elements.conf \$(top_srcdir)/click-buildtool
	(cd \$(top_srcdir); ./click-buildtool elem2export) < elements.conf > elements.cc
	\@rm -f elements.d

DEPFILES := \$(wildcard *.d)
ifneq (\$(DEPFILES),)
include \$(DEPFILES)
endif

install: click
	\$(mkinstalldirs) \$(bindir)
	\$(INSTALL) click \$(bindir)/click
install-man:

clean:
	rm -f *.d *.o click elements.mk elements.cc elements.conf
distclean: clean
	-rm -f Makefile

.PHONY: all clean distclean elemlist install install-man

EOF

close(F0);

system("sh -c \"cd $click_dir; CONFIG_FILES=$build_dir/Makefile ./config.status\"");


