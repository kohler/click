// -*- c-basic-offset: 4; related-file-name: "../include/click/driver.hh" -*-
/*
 * driver.cc -- support for packages
 * Eddie Kohler
 *
 * Copyright (c) 2001 Mazu Networks, Inc.
 * Copyright (c) 2003 International Computer Science Institute
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

#ifdef CLICK_LINUXMODULE
# define WANT_MOD_USE_COUNT 1	/* glue.hh should not define MOD_USE_COUNTs */
#endif

#include <click/package.hh>
#include <click/hashmap.hh>
#include <click/error.hh>

#if !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE)
# include <click/userutils.hh>
# include <unistd.h>
# include <cerrno>
# include <cstring>
# include <cstdlib>
#endif

#ifdef CLICK_TOOL
# include "lexert.hh"
# include "routert.hh"
#else
# include <click/lexer.hh>
#endif

#ifdef CLICK_USERLEVEL
# include <click/driver.hh>
# include <click/straccum.hh>
#endif


// GENERIC PACKAGE SUPPORT

struct ClickProvision {
    CLICK_NAME(String) name;
    bool loaded : 1;
    int provided;
};
static int nprovisions;
static int provisions_cap;
static ClickProvision *provisions;

static ClickProvision *
find_provision(const CLICK_NAME(String) &name)
{
    for (int i = 0; i < nprovisions; i++)
	if (provisions[i].name == name)
	    return &provisions[i];

    // otherwise, create new ClickProvision
    ClickProvision *npf;
    if (nprovisions >= provisions_cap) {
	int n = (nprovisions ? nprovisions * 2 : 4);
	if (!(npf = new ClickProvision[n]))
	    return 0;
	for (int i = 0; i < nprovisions; i++)
	    npf[i] = provisions[i];
	provisions_cap = n;
	delete[] provisions;
	provisions = npf;
    } else
	npf = provisions;

    npf[nprovisions].name = name;
    npf[nprovisions].loaded = false;
    npf[nprovisions].provided = 0;
    return &npf[nprovisions++];
}


extern "C" void
click_provide(const char *package)
{
    ClickProvision *p = find_provision(package);
    if (p) {
#ifndef CLICK_TOOL
	MOD_INC_USE_COUNT;
#endif
	p->provided++;
    }
}

extern "C" void
click_unprovide(const char *package)
{
    ClickProvision *p = find_provision(package);
    if (p && p->provided > 0) {
#ifndef CLICK_TOOL
	MOD_DEC_USE_COUNT;
#endif
	p->provided--;
    }
}

extern "C" bool
click_has_provision(const char *package)
{
    ClickProvision *p = find_provision(package);
    return (p && p->provided > 0);
}

extern "C" void
click_public_packages(CLICK_NAME(Vector)<CLICK_NAME(String)> &v)
{
    for (int i = 0; i < nprovisions; i++)
	if (provisions[i].provided > 0 && provisions[i].name && provisions[i].name[0] != '@')
	    v.push_back(provisions[i].name);
}

#ifdef CLICK_LINUXMODULE
extern "C" void
click_cleanup_packages()
{
    delete[] provisions;
    provisions = 0;
    nprovisions = provisions_cap = 0;
}
#endif


#if defined(CLICK_USERLEVEL) || (HAVE_DYNAMIC_LINKING && !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE))
CLICK_DECLS

static int
archive_index(const Vector<ArchiveElement> *archive, const String &what)
{
    if (archive)
	for (int i = 0; i < archive->size(); i++)
	    if (archive->at(i).name == what)
		return i;
    return -1;
}

CLICK_ENDDECLS
#endif


#if HAVE_DYNAMIC_LINKING && !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE)
CLICK_DECLS

static String::Initializer crap_initializer;
static String click_compile_prog, tmpdir;

static bool
check_tmpdir(const Vector<ArchiveElement> *archive, ErrorHandler *errh)
{
    // change to temporary directory
    if (tmpdir)
	return tmpdir;
    if (!(tmpdir = click_mktmpdir(errh)))
	return String();

    // find compile program
    if (!click_compile_prog) {
	click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);
	if (!click_compile_prog)
	    return String();
    }

    // store .hh files in temporary directory
    if (archive)
	for (int i = 0; i < archive->size(); i++)
	    if ((*archive)[i].name.substring(-3) == ".hh") {
		String filename = tmpdir + (*archive)[i].name;
		FILE *f = fopen(filename.c_str(), "w");
		if (!f)
		    errh->warning("%s: %s", filename.c_str(), strerror(errno));
		else {
		    fwrite((*archive)[i].data.data(), 1, (*archive)[i].data.length(), f);
		    fclose(f);
		}
	    }

    return tmpdir;
}

static String
compile_archive_file(String package, const Vector<ArchiveElement> *archive, int ai, ErrorHandler *errh)
{
    if (!check_tmpdir(archive, errh))
	return String();

#ifdef CLICK_TOOL
    String package_file = package + ".to";
    String target = "tool";
#else
    String package_file = package + ".uo";
    String target = "user";
#endif
  
    ContextErrorHandler cerrh
	(errh, "While compiling package `" + package_file + "':");

    // write .cc file
    const ArchiveElement &ae = archive->at(ai);
    String filename = tmpdir + ae.name;
    FILE *f = fopen(filename.c_str(), "w");
    if (!f) {
	cerrh.error("%s: %s", filename.c_str(), strerror(errno));
	return String();
    }
    fwrite(ae.data.data(), 1, ae.data.length(), f);
    fclose(f);
  
    // run click-compile
    String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=" + target + " --package=" + package_file + " " + filename;
    errh->message("%s", compile_command.cc());
    int compile_retval = system(compile_command.cc());
    if (compile_retval == 127)
	cerrh.error("could not run `%s'", compile_command.cc());
    else if (compile_retval < 0)
	cerrh.error("could not run `%s': %s", compile_command.cc(), strerror(errno));
    else if (compile_retval != 0)
	cerrh.error("`%s' failed", compile_command.cc());
    else
	return tmpdir + package_file;

    return String();
}

void
clickdl_load_requirement(String name, const Vector<ArchiveElement> *archive, ErrorHandler *errh)
{
    ClickProvision *p = find_provision(name);
    if (!p || p->loaded)
	return;

    ContextErrorHandler cerrh(errh, "While loading package `" + name + "':");

#ifdef CLICK_TOOL
    String suffix = ".to", cxx_suffix = ".t.cc";
#else
    String suffix = ".uo", cxx_suffix = ".u.cc";
#endif
    String package;
  
    // check archive
    int ai;
    if ((ai = archive_index(archive, name + suffix)) >= 0) {
	if (!check_tmpdir(archive, &cerrh))
	    return;
	package = tmpdir + "/" + name + suffix;
	FILE *f = fopen(package.c_str(), "wb");
	if (!f) {
	    cerrh.error("cannot open `%s': %s", package.c_str(), strerror(errno));
	    package = String();
	} else {
	    const ArchiveElement &ae = archive->at(ai);
	    fwrite(ae.data.data(), 1, ae.data.length(), f);
	    fclose(f);
	}
    } else if ((ai = archive_index(archive, name + cxx_suffix)) >= 0)
	package = compile_archive_file(name, archive, ai, &cerrh);
    else if ((ai = archive_index(archive, name + ".cc")) >= 0)
	package = compile_archive_file(name, archive, ai, &cerrh);
    else {
	// search path
	package = clickpath_find_file(name + suffix, "lib", CLICK_LIBDIR);
	if (!package)
	    package = clickpath_find_file(name + ".o", "lib", CLICK_LIBDIR);
	if (!package)
	    cerrh.error("can't find required package `%s%s'\nin CLICKPATH or `%s'", name.cc(), suffix.cc(), CLICK_LIBDIR);
    }

    p->loaded = true;
    if (package)
	clickdl_load_package(package, &cerrh);
}

CLICK_ENDDECLS
#endif /* HAVE_DYNAMIC_LINKING && !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE) */


#ifdef CLICK_USERLEVEL
extern void click_export_elements(CLICK_NAME(Lexer) *);

CLICK_DECLS
namespace {

class RequireLexerExtra : public LexerExtra { public:
    RequireLexerExtra(const Vector<ArchiveElement> *a) : _archive(a) { }
    void require(String, ErrorHandler *);
  private:
    const Vector<ArchiveElement> *_archive;
};

void
RequireLexerExtra::require(String name, ErrorHandler *errh)
{
#ifdef HAVE_DYNAMIC_LINKING
    if (!click_has_provision(name.c_str()))
	clickdl_load_requirement(name, _archive, errh);
#endif
    if (!click_has_provision(name.c_str()))
	errh->error("requirement `%s' not available", name.cc());
}

}


static Lexer *click_lexer;

extern "C" int
click_add_element_type(const char *ename, Element *e)
{
    return click_lexer->add_element_type(ename, e);
}

extern "C" void
click_remove_element_type(int which)
{
    click_lexer->remove_element_type(which);
}


enum { GH_CLASSES, GH_PACKAGES };

static String
read_handler(Element *, void *thunk)
{
    Vector<String> v;
    switch (reinterpret_cast<intptr_t>(thunk)) {
      case GH_CLASSES:
	if (click_lexer)
	    click_lexer->element_type_names(v);
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
    String::static_initialize();
    cp_va_static_initialize();

    ErrorHandler::static_initialize(new FileErrorHandler(stderr, ""));

    Router::static_initialize();
    CLICK_DEFAULT_PROVIDES;

    Router::add_read_handler(0, "classes", read_handler, (void *)GH_CLASSES);
    Router::add_read_handler(0, "packages", read_handler, (void *)GH_PACKAGES);
}

void
click_static_cleanup()
{
    delete click_lexer;
    click_lexer = 0;
    
    Router::static_cleanup();
    ErrorHandler::static_cleanup();
    cp_va_static_cleanup();
    String::static_cleanup();
}

Router *
click_read_router(String filename, bool is_expr, ErrorHandler *errh, bool initialize)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    int before = errh->nerrors();

    // read file
    String config_str;
    if (is_expr) {
	config_str = filename;
	filename = "<expr>";
    } else {
	config_str = file_string(filename, errh);
	if (!filename || filename == "-")
	    filename = "<stdin>";
    }
    if (errh->nerrors() > before)
	return 0;

    // find config string in archive
    Vector<ArchiveElement> archive;
    if (config_str.length() != 0 && config_str[0] == '!') {
	separate_ar_string(config_str, archive, errh);
	int i = archive_index(&archive, "config");
	if (i >= 0)
	    config_str = archive[i].data;
	else {
	    errh->error("%s: archive has no 'config' section", filename.c_str());
	    return 0;
	}
    }

    // lex
    if (!click_lexer) {
	click_lexer = new Lexer;
	click_export_elements(click_lexer);
    }
    RequireLexerExtra lextra(&archive);
    int cookie = click_lexer->begin_parse(config_str, filename, &lextra, errh);
    while (click_lexer->ystatement())
	/* do nothing */;
    Router *router = click_lexer->create_router();
    click_lexer->end_parse(cookie);

    // initialize if requested
    if (initialize)
	if (errh->nerrors() > before || router->initialize(errh) < 0) {
	    delete router;
	    return 0;
	}
    
    return router;
}

CLICK_ENDDECLS
#endif
