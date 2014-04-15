// -*- c-basic-offset: 4; related-file-name: "../include/click/driver.hh" -*-
/*
 * driver.cc -- support for packages
 * Eddie Kohler
 *
 * Copyright (c) 2001 Mazu Networks, Inc.
 * Copyright (c) 2003 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/pathvars.h>

#if CLICK_LINUXMODULE
# define WANT_MOD_USE_COUNT 1	/* glue.hh should not define MOD_USE_COUNTs */
#endif

#include <click/driver.hh>
#include <click/package.hh>
#include <click/error.hh>
#include <click/confparse.hh>

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# include <click/userutils.hh>
# include <click/straccum.hh>
# include <unistd.h>
# include <errno.h>
# include <string.h>
# include <stdlib.h>
#endif

#if CLICK_TOOL
# include "lexert.hh"
# include "routert.hh"
# include "landmarkt.hh"
# include <click/confparse.hh>
#else
# include <click/lexer.hh>
#endif

#if CLICK_USERLEVEL || CLICK_MINIOS
# include <click/master.hh>
# include <click/notifier.hh>
# include <click/straccum.hh>
# include <click/nameinfo.hh>
# include <click/bighashmap_arena.hh>
#endif

#if HAVE_DYNAMIC_LINKING && !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# define CLICK_PACKAGE_LOADED	1
#endif

// GENERIC PACKAGE SUPPORT

struct ClickProvision {
    CLICK_NAME(String) name;
#if CLICK_PACKAGE_LOADED
    bool loaded : 1;
#endif
    int provided;
};
static int nprovisions;
static int provisions_cap;
static ClickProvision *provisions;

static ClickProvision *
find_provision(const CLICK_NAME(String) &name, int add)
{
    ClickProvision *pf = 0;

    for (int i = 0; i < nprovisions; i++)
	if (provisions[i].name == name)
	    return &provisions[i];
	else if (add && provisions[i].provided == 0
#if CLICK_PACKAGE_LOADED
		 && !provisions[i].loaded
#endif
		 )
	    pf = &provisions[i];

    if (!add)
	return 0;

    // otherwise, create new ClickProvision
    if (!pf) {
	if (nprovisions >= provisions_cap) {
	    int n = (nprovisions ? nprovisions * 2 : 4);
	    ClickProvision *npf = new ClickProvision[n];
	    if (!npf)
		return 0;
	    for (int i = 0; i < nprovisions; i++)
		npf[i] = provisions[i];
	    provisions_cap = n;
	    delete[] provisions;
	    provisions = npf;
	}
	pf = &provisions[nprovisions++];
    }

    pf->name = name;
#if CLICK_PACKAGE_LOADED
    pf->loaded = false;
#endif
    pf->provided = 0;
    return pf;
}


extern "C" void
click_provide(const char *package)
{
    ClickProvision *p = find_provision(package, 1);
    if (p)
	p->provided++;
}

extern "C" void
click_unprovide(const char *package)
{
    ClickProvision *p = find_provision(package, 0);
    if (p && p->provided > 0)
	p->provided--;
}

extern "C" bool
click_has_provision(const char *package)
{
    ClickProvision *p = find_provision(package, 0);
    return (p && p->provided > 0);
}

extern "C" void
click_public_packages(CLICK_NAME(Vector)<CLICK_NAME(String)> &v)
{
    for (int i = 0; i < nprovisions; i++)
	if (provisions[i].provided > 0 && provisions[i].name && provisions[i].name[0] != '@')
	    v.push_back(provisions[i].name);
}

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
extern "C" void
click_cleanup_packages()
{
    delete[] provisions;
    provisions = 0;
    nprovisions = provisions_cap = 0;
}
#endif


#if CLICK_PACKAGE_LOADED || CLICK_TOOL
CLICK_DECLS

static String *click_buildtool_prog, *tmpdir;

static bool
check_tmpdir(const Vector<ArchiveElement> &archive, bool populate_tmpdir,
	     bool &tmpdir_populated, ErrorHandler *errh)
{
    // change to temporary directory
    if (!tmpdir)
	tmpdir = new String(click_mktmpdir(errh));
    if (!*tmpdir)
	return *tmpdir;

    // store .h/.hh/.hxx files in temporary directory
    if (populate_tmpdir && !tmpdir_populated) {
	tmpdir_populated = true;
	for (int i = 0; i < archive.size(); i++) {
	    const ArchiveElement &ae = archive[i];
	    if (ae.name.substring(-3) == ".hh"
		|| ae.name.substring(-2) == ".h"
		|| ae.name.substring(-4) == ".hxx") {
		String filename = *tmpdir + ae.name;
		FILE *f = fopen(filename.c_str(), "w");
		if (!f)
		    errh->warning("%s: %s", filename.c_str(), strerror(errno));
		else {
		    ignore_result(fwrite(ae.data.data(), 1, ae.data.length(), f));
		    fclose(f);
		}
	    }
	}
    }

    return *tmpdir;
}

String
click_compile_archive_file(const Vector<ArchiveElement> &archive,
			   const ArchiveElement *ae,
			   String package,
			   const String &target, int quiet,
			   bool &tmpdir_populated, ErrorHandler *errh)
{
    // create and populate temporary directory
    if (!check_tmpdir(archive, true, tmpdir_populated, errh))
	return String();

    // find compile program
    if (!click_buildtool_prog)
	click_buildtool_prog = new String(clickpath_find_file("click-buildtool", "bin", CLICK_BINDIR, errh));
    if (!*click_buildtool_prog)
	return *click_buildtool_prog;

    String package_file = package;
    if (target == "tool")
	package_file += ".to";
    else if (target == "userlevel")
	package_file += ".uo";
    else if (target == "linuxmodule")
	package_file += ".ko";
    else if (target == "bsdmodule")
	package_file += ".bo";

    ContextErrorHandler cerrh
	(errh, "While compiling package %<%s%>:", package_file.c_str());

    // write .cc file
    String filename = ae->name;
    String filepath = *tmpdir + filename;
    FILE *f = fopen(filepath.c_str(), "w");
    if (!f) {
	cerrh.error("%s: %s", filepath.c_str(), strerror(errno));
	return String();
    }
    ignore_result(fwrite(ae->data.data(), 1, ae->data.length(), f));
    fclose(f);

    // prepare click-buildtool makepackage
    StringAccum compile_command;
    compile_command << *click_buildtool_prog << " makepackage "
		    << (quiet > 0 ? "-q " : (quiet < 0 ? "-V " : ""))
		    << "-C " << *tmpdir << " -t " << target;

    // check for compile flags
    const char *ss = ae->data.begin();
    const char *send = ae->data.end();
    for (int i = 0; i < 5 && ss < send; i++) {
	// grab first line
	const char *eb = ss;
	const char *el = ss;
	while (el < send && *el != '\n' && *el != '\r')
	    el++;
	if (el + 1 < send && *el == '\r' && el[1] == '\n')
	    ss = el + 2;
	else if (el < send)
	    ss = el + 1;

	// eat end-of-line space
	while (eb < el && isspace((unsigned char) el[-1]))
	    el--;

	// check for compile arguments signalled by "click-compile"
	if (eb + 20 < send && memcmp(eb, "/** click-compile:", 18) == 0
	    && el[-1] == '/' && el[-2] == '*') {
	    for (const char *x = eb + 18; x < el - 2; x++)
		if (*x == '\'' || *x == '\"' || *x == ';' || *x == '|' || *x == '>' || *x == '<' || *x == '&' || *x == '*')
		    goto bad_click_compile;
	    compile_command << ' ' << ae->data.substring(eb + 18, el - 2);
	  bad_click_compile: ;
	}
    }

    // finish compile_command
    compile_command << ' ' << package << ' ' << filename << " 1>&2";
    if (quiet <= 0)
	errh->message("%s", compile_command.c_str());
    int compile_retval = system(compile_command.c_str());
    if (compile_retval == 127)
	cerrh.error("could not run %<%s%>", compile_command.c_str());
    else if (compile_retval < 0)
	cerrh.error("could not run %<%s%>: %s", compile_command.c_str(), strerror(errno));
    else if (compile_retval != 0)
	cerrh.error("%<%s%> failed", compile_command.c_str());
    else
	return *tmpdir + package_file;

    return String();
}

# if CLICK_PACKAGE_LOADED
void
clickdl_load_requirement(String name, const Vector<ArchiveElement> *archive, ErrorHandler *errh)
{
    ClickProvision *p = find_provision(name, 1);
    if (!p || p->loaded)
	return;

    ContextErrorHandler cerrh(errh, "While loading package %<%s%>:", name.c_str());
    bool tmpdir_populated = false;

#ifdef CLICK_TOOL
    String suffix = ".to", cxx_suffix = ".t.cc", target = "tool";
#else
    String suffix = ".uo", cxx_suffix = ".u.cc", target = "userlevel";
#endif
    String package;

    // check archive
    const ArchiveElement *ae = 0;
    if (archive && (ae = ArchiveElement::find(*archive, name + suffix))) {
	if (!check_tmpdir(*archive, false, tmpdir_populated, &cerrh))
	    return;
	package = *tmpdir + "/" + name + suffix;
	FILE *f = fopen(package.c_str(), "wb");
	if (!f) {
	    cerrh.error("cannot open %<%s%>: %s", package.c_str(), strerror(errno));
	    package = String();
	} else {
	    ignore_result(fwrite(ae->data.data(), 1, ae->data.length(), f));
	    fclose(f);
	}
    } else if (archive && (ae = ArchiveElement::find(*archive, name + cxx_suffix)))
	package = click_compile_archive_file(*archive, ae, name, target, true, tmpdir_populated, &cerrh);
    else if (archive && (ae = ArchiveElement::find(*archive, name + ".cc")))
	package = click_compile_archive_file(*archive, ae, name, target, true, tmpdir_populated, &cerrh);
    else {
	// search path
	package = clickpath_find_file(name + suffix, "lib", CLICK_LIBDIR);
	if (!package)
	    package = clickpath_find_file(name + ".o", "lib", CLICK_LIBDIR);
	if (!package)
	    cerrh.error("can't find required package %<%s%s%>\nin CLICKPATH or %<%s%>", name.c_str(), suffix.c_str(), CLICK_LIBDIR);
    }

    p->loaded = true;
    if (package)
	clickdl_load_package(package, &cerrh);
}
# endif /* CLICK_PACKAGE_LOADED */

CLICK_ENDDECLS
#endif /* CLICK_PACKAGE_LOADED || CLICK_TOOL */


#if defined(CLICK_USERLEVEL) || defined(CLICK_MINIOS)
extern void click_export_elements();
extern void click_unexport_elements();

CLICK_DECLS
namespace {

class RequireLexerExtra : public LexerExtra { public:
    RequireLexerExtra(const Vector<ArchiveElement> *a) : _archive(a) { }
    void require(String type, String value, ErrorHandler *errh);
  private:
    const Vector<ArchiveElement> *_archive;
};

void
RequireLexerExtra::require(String type, String value, ErrorHandler *errh)
{
# ifdef HAVE_DYNAMIC_LINKING
    if (type.equals("package", 7) && !click_has_provision(value.c_str()))
	clickdl_load_requirement(value, _archive, errh);
# endif
    if (type.equals("package", 7) && !click_has_provision(value.c_str()))
	errh->error("requirement %<%s%> not available", value.c_str());
}

}


static Lexer *_click_lexer;

Lexer *
click_lexer()
{
    if (!_click_lexer)
	_click_lexer = new Lexer;
    return _click_lexer;
}

extern "C" int
click_add_element_type(const char *ename, Element *(*func)(uintptr_t), uintptr_t thunk)
{
    assert(ename);
    if (Lexer *l = click_lexer())
	return l->add_element_type(ename, func, thunk);
    else
	return -99;
}

extern "C" int
click_add_element_type_stable(const char *ename, Element *(*func)(uintptr_t), uintptr_t thunk)
{
    assert(ename);
    if (Lexer *l = click_lexer())
	return l->add_element_type(String::make_stable(ename), func, thunk);
    else
	return -99;
}

extern "C" void
click_remove_element_type(int which)
{
    if (_click_lexer)
	_click_lexer->remove_element_type(which);
}


enum { GH_CLASSES, GH_PACKAGES };

static String
read_handler(Element *, void *thunk)
{
    Vector<String> v;
    switch (reinterpret_cast<intptr_t>(thunk)) {
      case GH_CLASSES:
	if (_click_lexer)
	    _click_lexer->element_type_names(v);
	break;
      case GH_PACKAGES:
	click_public_packages(v);
	break;
      default:
	return "<error>\n";
    }

    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
	sa << v[i] << '\n';
    return sa.take_string();
}

void
click_static_initialize()
{
    NameInfo::static_initialize();
    cp_va_static_initialize();

    ErrorHandler::static_initialize(new FileErrorHandler(stderr, ""));

    Router::static_initialize();
    NotifierSignal::static_initialize();
    CLICK_DEFAULT_PROVIDES;

    Router::add_read_handler(0, "classes", read_handler, (void *)GH_CLASSES);
    Router::add_read_handler(0, "packages", read_handler, (void *)GH_PACKAGES);

    click_export_elements();
}

void
click_static_cleanup()
{
    delete _click_lexer;
    _click_lexer = 0;

#if !(CLICK_LINUXMODULE || CLICK_BSDMODULE)
    delete[] provisions;
    provisions = 0;
    nprovisions = provisions_cap = 0;
#endif

    click_unexport_elements();

    Router::static_cleanup();
    Packet::static_cleanup();
    ErrorHandler::static_cleanup();
    cp_va_static_cleanup();
    NameInfo::static_cleanup();
    HashMap_ArenaFactory::static_cleanup();

# ifdef HAVE_DYNAMIC_LINKING
    delete tmpdir;
    delete click_buildtool_prog;
    tmpdir = click_buildtool_prog = 0;
# endif /* HAVE_DYNAMIC_LINKING */
}

Router *
click_read_router(String filename, bool is_expr, ErrorHandler *errh, bool initialize, Master *master)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    int before = errh->nerrors();

    // read file
    String config_str;
    if (is_expr) {
	config_str = filename;
	filename = "config";
#ifdef CLICK_MINIOS
    } else {
        errh->error("MiniOS doesn't support loading configurations from files!");
#else
    } else {
	config_str = file_string(filename, errh);
	if (!filename || filename == "-")
	    filename = "<stdin>";
#endif
    }
    if (errh->nerrors() > before)
	return 0;

    // find config string in archive
    Vector<ArchiveElement> archive;
    if (config_str.length() != 0 && config_str[0] == '!') {
	ArchiveElement::parse(config_str, archive, errh);
	if (ArchiveElement *ae = ArchiveElement::find(archive, "config"))
	    config_str = ae->data;
	else {
	    errh->error("%s: archive has no %<config%> section", filename.c_str());
	    return 0;
	}
    }

    // lex
    Lexer *l = click_lexer();
    RequireLexerExtra lextra(&archive);
    int cookie = l->begin_parse(config_str, filename, &lextra, errh);
    while (!l->ydone())
	l->ystep();
    Router *router = l->create_router(master ? master : new Master(1));
    l->end_parse(cookie);

    // initialize if requested
    if (initialize)
	if (errh->nerrors() > before || router->initialize(errh) < 0) {
	    delete router;
	    return 0;
	}

    return router;
}

CLICK_ENDDECLS
#endif /* CLICK_USERLEVEL || CLICK_MINIOS */


#if CLICK_TOOL
CLICK_DECLS

void
click_static_initialize()
{
    cp_va_static_initialize();
    ErrorHandler::static_initialize(new FileErrorHandler(stderr, ""));
    LandmarkT::static_initialize();
}

CLICK_ENDDECLS
#endif
