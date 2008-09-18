// -*- c-basic-offset: 4 -*-
#ifndef CLICK_INSTALL_COMMON_HH
#define CLICK_INSTALL_COMMON_HH
#include <click/pathvars.h>	// defines HAVE_X_DRIVER symbols
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/error.hh>
#include "routert.hh"		// for StringMap

// check whether we are compiling for Linux or FreeBSD
#if !FOR_LINUXMODULE && !FOR_BSDMODULE
# if HAVE_LINUXMODULE_DRIVER && HAVE_BSDMODULE_DRIVER
#  error "Not sure whether to compile for linuxmodule or bsdmodule"
# elif !HAVE_LINUXMODULE_DRIVER && !HAVE_BSDMODULE_DRIVER
#  error "Must compile one of linuxmodule and bsdmodule"
# elif HAVE_LINUXMODULE_DRIVER
#  define FOR_LINUXMODULE 1
# else
#  define FOR_BSDMODULE 1
# endif
#endif

#if FOR_LINUXMODULE
# define OBJSUFFIX ".ko"
# define CXXSUFFIX ".k.cc"
# define COMPILETARGET "linuxmodule"
#elif FOR_BSDMODULE
# define OBJSUFFIX ".bo"
# define CXXSUFFIX ".b.cc"
# define COMPILETARGET "bsdmodule"
#endif

#if FOR_LINUXMODULE || FOR_BSDMODULE
extern String clickfs_dir;
extern String clickfs_prefix;
#endif

extern bool verbose;

bool adjust_clickfs_prefix();
bool read_package_file(String filename, StringMap &packages, ErrorHandler *);
bool read_active_modules(StringMap &packages, ErrorHandler *);
int remove_unneeded_packages(const StringMap &active, const StringMap &packages, ErrorHandler *);
int unload_click(ErrorHandler *);

#endif
