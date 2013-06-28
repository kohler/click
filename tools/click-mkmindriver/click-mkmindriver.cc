// -*- c-basic-offset: 4 -*-
/*
 * click-mkmindriver.cc -- produce a minimum Click driver Makefile setup
 * Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2004-2011 Regents of the University of California
 * Copyright (c) 2013 President and Fellows of Harvard College
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

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/driver.hh>
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include "elementmap.hh"
#include <click/clp.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define PACKAGE_OPT		306
#define DIRECTORY_OPT		307
#define KERNEL_OPT		308
#define USERLEVEL_OPT		309
#define ELEMENT_OPT		310
#define ALIGN_OPT		311
#define ALL_OPT			312
#define CHECK_OPT		313
#define VERBOSE_OPT		314
#define EXTRAS_OPT		315

static const Clp_Option options[] = {
  { "align", 'A', ALIGN_OPT, 0, 0 },
  { "all", 'a', ALL_OPT, 0, Clp_Negate },
  { "check", 0, CHECK_OPT, 0, Clp_Negate },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
  { "directory", 'd', DIRECTORY_OPT, Clp_ValString, 0 },
  { "elements", 'E', ELEMENT_OPT, Clp_ValString, 0 },
  { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
  { "extras", 0, EXTRAS_OPT, 0, Clp_Negate },
  { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, 0 }, // DEPRECATED
  { "linuxmodule", 'l', KERNEL_OPT, 0, 0 },
  { "package", 'p', PACKAGE_OPT, Clp_ValString, 0 },
  { "userlevel", 'u', USERLEVEL_OPT, 0, 0 },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate }
};

static const char *program_name;

static int driver = -1;
static HashTable<String, int> initial_requirements(-1);
static bool verbose = false;

void
short_usage()
{
  fprintf(stderr, "Usage: %s -p PKGNAME [OPTION]... [ROUTERFILE]...\n\
Try '%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
'Click-mkmindriver' generates a build environment for a minimal Click driver,\n\
which contains just the elements required to support one or more\n\
configurations. Run 'click-mkmindriver' in the relevant driver's build\n\
directory and supply a package name with the '-p PKG' option. Running\n\
'make MINDRIVER=PKG' will build a 'PKGclick' user-level driver or 'PKGclick.ko'\n\
kernel module.\n\
\n\
Usage: %s -p PKG [-lu] [OPTION]... [ROUTERFILE]...\n\
\n\
Options:\n\
  -p, --package PKG        Name of package is PKG.\n\
  -f, --file FILE          Read a router configuration from FILE.\n\
  -e, --expression EXPR    Use EXPR as a router configuration.\n\
  -a, --all                Add all element classes from following configs,\n\
                           even those in unused compound elements.\n\
  -l, --linuxmodule        Build Makefile for Linux kernel module driver.\n\
  -u, --userlevel          Build Makefile for user-level driver (default).\n\
  -d, --directory DIR      Put files in DIR. DIR must contain a 'Makefile'\n\
                           for the relevant driver. Default is '.'.\n\
  -E, --elements ELTS      Include element classes ELTS.\n\
      --no-extras          Don't include surplus often-useful elements.\n\
  -V, --verbose            Print progress information.\n\
  -C, --clickpath PATH     Use PATH for CLICKPATH.\n\
      --help               Print this message and exit.\n\
  -v, --version            Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


class Mindriver { public:

    Mindriver();

    void provide(const String&, ErrorHandler*);
    void require(const String&, ErrorHandler*);
    void add_source_file(const String&, ErrorHandler*);

    void add_router_requirements(RouterT*, const ElementMap&, ErrorHandler*);
    bool add_traits(const Traits&, const ElementMap&, ErrorHandler*);
    bool resolve_requirement(const String& requirement, const ElementMap& emap, ErrorHandler* errh, bool complain = true);
    void print_elements_conf(FILE*, String package, const ElementMap&, const String &top_srcdir);

    HashTable<String, int> _provisions;
    HashTable<String, int> _requirements;
    HashTable<String, int> _source_files;
    int _nrequirements;

};

Mindriver::Mindriver()
    : _provisions(-1), _requirements(-1), _source_files(-1), _nrequirements(0)
{
}

void
Mindriver::provide(const String& req, ErrorHandler* errh)
{
    if (verbose && _provisions[req] < 0)
	errh->message("providing %<%s%>", req.c_str());
    _provisions[req] = 1;
}

void
Mindriver::require(const String& req, ErrorHandler* errh)
{
    if (_provisions.get(req) < 0) {
	if (verbose && _requirements[req] < 0)
	    errh->message("requiring %<%s%>", req.c_str());
	_requirements[req] = 1;
	_nrequirements++;
    }
}

void
Mindriver::add_source_file(const String& fn, ErrorHandler* errh)
{
    if (verbose && _source_files[fn] < 0)
	errh->message("adding source file %<%s%>", fn.c_str());
    _source_files[fn] = 1;
}

void
Mindriver::add_router_requirements(RouterT* router, const ElementMap& default_map, ErrorHandler* errh)
{
    // find and parse elementmap
    ElementMap emap(default_map);
    emap.parse_requirement_files(router, CLICK_DATADIR, errh);

    // check whether suitable for driver
    if (!emap.driver_compatible(router, driver)) {
	errh->error("not compatible with %s driver; ignored", Driver::name(driver));
	return;
    }
    emap.set_driver(driver);

    StringAccum missing_sa;
    int nmissing = 0;

    HashTable<ElementClassT*, int> primitives(-1);
    router->collect_types(primitives);
    for (HashTable<ElementClassT*, int>::iterator i = primitives.begin(); i.live(); i++) {
	if (!i.key()->primitive())
	    continue;
	String tname = i.key()->name();
	if (!emap.has_traits(tname))
	    missing_sa << (nmissing++ ? ", " : "") << tname;
	else if (emap.package(tname))
	    /* do nothing; element was defined in a package */;
	else
	    require(tname, errh);
    }

    if (nmissing == 1)
	errh->fatal("cannot locate required element class %<%s%>\n(This may be due to a missing or out-of-date %<elementmap.xml%>.)", missing_sa.c_str());
    else if (nmissing > 1)
	errh->fatal("cannot locate these required element classes:\n  %s\n(This may be due to a missing or out-of-date %<elementmap.xml%>.)", missing_sa.c_str());
}

static void
handle_router(Mindriver& md, String filename_in, const ElementMap &default_map, ErrorHandler *errh)
{
    // decide if 'filename' should be flattened
    bool flattenable = (filename_in[0] != 'a');
    bool file_is_expr = (filename_in[1] == 'e');
    const char *filename = filename_in.c_str() + 2;

    // read file
    int before = errh->nerrors();
    RouterT *router = read_router(filename, file_is_expr, errh);
    if (file_is_expr)
	filename = "config";
    else if (!filename || strcmp(filename, "-") == 0)
	filename = "<stdin>";
    LandmarkErrorHandler lerrh(errh, filename);
    if (router && flattenable)
	router->flatten(&lerrh);
    if (router && errh->nerrors() == before)
	md.add_router_requirements(router, default_map, &lerrh);
    delete router;
}

bool
Mindriver::add_traits(const Traits& t, const ElementMap&, ErrorHandler* errh)
{
    if (t.source_file)
	add_source_file(t.source_file, errh);

    if (t.name)
	provide(t.name, errh);

    if (t.provisions) {
	Vector<String> args;
	cp_spacevec(t.provisions, args);
	for (String* s = args.begin(); s < args.end(); s++)
	    if (Driver::driver(*s) < 0)
		provide(*s, errh);
    }

    if (t.requirements) {
	Vector<String> args;
	cp_spacevec(t.requirements, args);
	for (String* s = args.begin(); s < args.end(); s++)
	    require(*s, errh);
    }

    return true;
}

bool
Mindriver::resolve_requirement(const String& requirement, const ElementMap& emap, ErrorHandler* errh, bool complain)
{
    LandmarkErrorHandler lerrh(errh, "resolving " + requirement);

    if (_provisions.get(requirement) > 0)
	return true;

    int try_name_emapi = emap.traits_index(requirement);
    if (try_name_emapi > 0) {
	add_traits(emap.traits_at(try_name_emapi), emap, &lerrh);
	return true;
    }

    for (int i = 1; i < emap.size(); i++)
	if (emap.traits_at(i).provides(requirement)) {
	    add_traits(emap.traits_at(i), emap, &lerrh);
	    return true;
	}

    // check for '|' requirements
    const char *begin = requirement.begin(), *bar;
    while ((bar = find(begin, requirement.end(), '|')) < requirement.end()) {
	if (resolve_requirement(requirement.substring(begin, bar), emap, errh, false))
	    return true;
	begin = bar + 1;
    }

    if (complain)
	errh->error("cannot satisfy requirement %<%s%>", requirement.c_str());
    return false;
}

void
Mindriver::print_elements_conf(FILE *f, String package, const ElementMap &emap, const String &top_srcdir)
{
    Vector<String> sourcevec;
    for (HashTable<String, int>::iterator iter = _source_files.begin();
	 iter.live();
	 iter++) {
	iter.value() = sourcevec.size();
	sourcevec.push_back(iter.key());
    }

    Vector<String> headervec(sourcevec.size(), String());
    Vector<String> classvec(sourcevec.size(), String());
    HashTable<String, int> statichash(0);

    // collect header file and C++ element class definitions from emap
    for (int i = 1; i < emap.size(); i++) {
	const Traits &elt = emap.traits_at(i);
	int sourcei = _source_files.get(elt.source_file);
	if (sourcei >= 0) {
	    // track ELEMENT_LIBS
	    // ah, if only I had regular expressions
	    if (!headervec[sourcei] && elt.libs) {
		StringAccum sa;
		sa << " -!lib";
		for (const char *x = elt.libs.begin(); x != elt.libs.end(); x++)
		    if (isspace((unsigned char) *x)) {
			sa << ';';
		    skipspace:
			while (x + 1 != elt.libs.end() && isspace((unsigned char) x[1]))
			    x++;
		    } else if (x + 1 != elt.libs.end() && *x == '-' && x[1] == 'L') {
			sa << '-' << 'L';
			x++;
			goto skipspace;
		    } else
			sa << *x;
		classvec[sourcei] += sa.take_string();
	    }
	    // remember header file
	    headervec[sourcei] = elt.header_file;
	    // remember name
	    if (elt.name && !elt.noexport)
		classvec[sourcei] += " " + elt.cxx + "-" + elt.name;
	    // remember static methods
	    if (elt.methods && !statichash[elt.cxx]) {
		statichash[elt.cxx] = 1;
		Vector<String> ms;
		cp_spacevec(elt.methods, ms);
		for (String *m = ms.begin(); m != ms.end(); m++)
		    if (*m == "static_initialize")
			classvec[sourcei] += " " + elt.cxx + "-!si";
		    else if (*m == "static_cleanup")
			classvec[sourcei] += " " + elt.cxx + "-!sc";
	    }
	}
    }

    // output data
    time_t now = time(0);
    const char *date_str = ctime(&now);
    fprintf(f, "# Generated by 'click-mkmindriver -p %s' on %s", package.c_str(), date_str);
    for (int i = 0; i < sourcevec.size(); i++)
	if (headervec[i]) {
	    String classstr(classvec[i].begin() + 1, classvec[i].end());
	    if (headervec[i][0] != '\"' && headervec[i][0] != '<')
		fprintf(f, "%s%s\t\"%s%s\"\t%s\n", top_srcdir.c_str(), sourcevec[i].c_str(), top_srcdir.c_str(), headervec[i].c_str(), classstr.c_str());
	    else
		fprintf(f, "%s%s\t%s\t%s\n", top_srcdir.c_str(), sourcevec[i].c_str(), headervec[i].c_str(), classstr.c_str());
	}
}

static String
analyze_makefile(const String &directory, ErrorHandler *errh)
{
    int before = errh->nerrors();

    String fn = directory + "Makefile";
    String text = file_string(fn, errh);
    if (before != errh->nerrors())
	return String();

    String expectation = String("\n## Click ") + Driver::requirement(driver) + " driver Makefile ##\n";
    if (text.find_left(expectation) < 0) {
	errh->error("%s lacks magic string\n(Does this directory have a Makefile for Click%,s %s driver?)", fn.c_str(), Driver::name(driver));
	return String();
    }

    int top_srcdir_pos = text.find_left("\ntop_srcdir := ");
    if (top_srcdir_pos < 0) {
	errh->error("%s lacks top_srcdir variable", fn.c_str());
	return String();
    }
    int top_srcdir_end = text.find_left('\n', top_srcdir_pos + 1);
    String top_srcdir = text.substring(top_srcdir_pos + 15, top_srcdir_end - (top_srcdir_pos + 15));
    if (top_srcdir.back() != '/')
	top_srcdir += '/';
    return top_srcdir;
}

int
main(int argc, char **argv)
{
    click_static_initialize();
    CLICK_DEFAULT_PROVIDES;
    ErrorHandler *errh = ErrorHandler::default_handler();
    LandmarkErrorHandler arg_lerrh(errh, "argument requirements");

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
    Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
    program_name = Clp_ProgramName(clp);

    Vector<String> router_filenames;
    String specifier = "x";
    const char *package_name = 0;
    String directory;
    bool check = true;
    bool extras = true;

    Mindriver md;

    while (1) {
	int opt = Clp_Next(clp);
	switch (opt) {

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case VERSION_OPT:
	    printf("click-mkmindriver (Click) %s\n", CLICK_VERSION);
	    printf("Copyright (c) 2001 Massachusetts Institute of Technology\n\
Copyright (c) 2001 International Computer Science Institute\n\
Copyright (c) 2004-2011 Regents of the University of California\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->vstr);
	    break;

	  case KERNEL_OPT:
	    driver = Driver::LINUXMODULE;
	    break;

	  case USERLEVEL_OPT:
	    driver = Driver::USERLEVEL;
	    break;

	  case PACKAGE_OPT:
	    package_name = clp->vstr;
	    break;

	  case DIRECTORY_OPT:
	    directory = clp->vstr;
	    if (directory.length() && directory.back() != '/')
		directory += "/";
	    break;

	  case ALL_OPT:
	    specifier = (clp->negated ? "x" : "a");
	    break;

	case CHECK_OPT:
	    check = !clp->negated;
	    break;

	  case ELEMENT_OPT: {
	      Vector<String> elements;
	      cp_spacevec(clp->vstr, elements);
	      for (String *e = elements.begin(); e < elements.end(); e++)
		  md.require(*e, &arg_lerrh);
	      break;
	  }

	  case ALIGN_OPT:
	    break;

	  case EXTRAS_OPT:
	    extras = !clp->negated;
	    break;

	  case VERBOSE_OPT:
	    verbose = !clp->negated;
	    break;

	  case ROUTER_OPT:
	  router_file:
	    router_filenames.push_back(specifier + String("f") + clp->vstr);
	    break;

	  case Clp_NotOption:
	    if (!click_maybe_define(clp->vstr, &arg_lerrh))
		goto router_file;
	    break;

	  case EXPRESSION_OPT:
	    router_filenames.push_back(specifier + String("e") + clp->vstr);
	    break;

	  case Clp_BadOption:
	    short_usage();
	    exit(1);
	    break;

	  case Clp_Done:
	    goto done;

	}
    }

  done:
    if (driver < 0)
	driver = Driver::USERLEVEL;
    if (!package_name)
	errh->fatal("fatal error: no package name specified\nPlease supply the %<-p PKG%> option.");
    if (extras) {
	md.require("Align", errh);
	md.require("IPNameInfo", errh);
    }

    ElementMap default_emap;
    if (!default_emap.parse_default_file(CLICK_DATADIR, errh))
	default_emap.report_file_not_found(CLICK_DATADIR, false, errh);

    for (int i = 0; i < router_filenames.size(); i++)
	handle_router(md, router_filenames[i], default_emap, errh);

    if (md._nrequirements == 0)
	errh->fatal("no elements required");

    // add types that are always required
    {
	LandmarkErrorHandler lerrh(errh, "default requirements");
	md.require("AddressInfo", &lerrh);
	md.require("AlignmentInfo", &lerrh);
	md.require("Error", &lerrh);
	md.require("PortInfo", &lerrh);
	md.require("ScheduleInfo", &lerrh);
	if (driver == Driver::USERLEVEL)
	    md.require("ControlSocket", &lerrh);
    }

    // add initial provisions
    default_emap.set_driver(driver);
    md.provide(Driver::requirement(driver), errh);

    // all default provisions are stored in elementmap index 0
    md.add_traits(default_emap.traits_at(0), default_emap, errh);

    // now, loop over requirements until closure
    while (1) {
	HashTable<String, int> old_reqs(-1);
	old_reqs.swap(md._requirements);

	for (HashTable<String, int>::iterator iter = old_reqs.begin(); iter.live(); iter++)
	    md.resolve_requirement(iter.key(), default_emap, errh);

	if (!md._requirements.size())
	    break;
    }

    if (errh->nerrors() > 0)
	exit(1);

    // Print elements_PKG.conf
    String top_srcdir = analyze_makefile(directory, (check ? errh : ErrorHandler::silent_handler()));

    if (errh->nerrors() == 0) {
	String fn = directory + String("elements_") + package_name + ".conf";
	errh->message("Creating %s...", fn.c_str());
	FILE *f = fopen(fn.c_str(), "w");
	if (!f)
	    errh->fatal("%s: %s", fn.c_str(), strerror(errno));
	md.print_elements_conf(f, package_name, default_emap, top_srcdir);
	fclose(f);
    }

    // Final message
    if (errh->nerrors() == 0) {
	if (driver == Driver::USERLEVEL)
	    errh->message("Build %<%sclick%> with %<make MINDRIVER=%s%>.", package_name, package_name);
	else
	    errh->message("Build %<%sclick.ko%> with %<make MINDRIVER=%s%>.", package_name, package_name);
	return 0;
    } else
	exit(1);
}
