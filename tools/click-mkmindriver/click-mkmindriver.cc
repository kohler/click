// -*- c-basic-offset: 4 -*-
/*
 * click-mkmindriver.cc -- flatten a Click configuration (remove compounds)
 * Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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

static Clp_Option options[] = {
  { "align", 'A', ALIGN_OPT, 0, 0 },
  { "all", 'a', ALL_OPT, 0, Clp_Negate },
  { "check", 0, CHECK_OPT, 0, Clp_Negate },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "directory", 'd', DIRECTORY_OPT, Clp_ArgString, 0 },
  { "elements", 'E', ELEMENT_OPT, Clp_ArgString, 0 },
  { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, Clp_Negate },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, 0 },
  { "linuxmodule", 0, KERNEL_OPT, 0, 0 },
  { "package", 'p', PACKAGE_OPT, Clp_ArgString, 0 },
  { "userlevel", 'u', USERLEVEL_OPT, 0, 0 },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate }
};

static const char *program_name;
static String::Initializer string_initializer;

static int driver = -1;
static HashMap<String, int> initial_requirements(-1);
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
'Click-mkmindriver' produces a Makefile that builds a minimum Click driver\n\
for a set of router configurations. This driver contains just the elements\n\
those configurations require. Run 'click-mkmindriver' in the relevant driver's\n\
build directory and supply a package name with the '-p PKG' option. The\n\
resulting Makefile is called 'Makefile.PKG'; it will build either a 'PKGclick'\n\
user-level driver or a 'PKGclick.o' kernel module.\n\
\n\
Usage: %s -p PKG [OPTION]... [ROUTERFILE]...\n\
\n\
Options:\n\
  -p, --package PKG        Name of package is PKG.\n\
  -f, --file FILE          Read a router configuration from FILE.\n\
  -e, --expression EXPR    Use EXPR as a router configuration.\n\
  -a, --all                Add all element classes from following configs,\n\
                           even those in unused compound elements.\n\
  -k, --linuxmodule        Build Makefile for Linux kernel module driver.\n\
  -u, --userlevel          Build Makefile for user-level driver (default).\n\
  -d, --directory DIR      Put files in DIR. DIR must contain a 'Makefile'\n\
                           for the relevant driver. Default is '.'.\n\
  -E, --elements ELTS      Include element classes ELTS.\n\
  -A, --align              Include element classes required by click-align.\n\
      --no-file            Don't read a configuration from standard input.\n\
      --no-check           Don't check the directory for a driver Makefile.\n\
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
    void print_elements_conf(FILE*, String package, const ElementMap&);
    
    HashMap<String, int> _provisions;
    HashMap<String, int> _requirements;
    HashMap<String, int> _source_files;
    
};

Mindriver::Mindriver()
    : _provisions(-1), _requirements(-1), _source_files(-1)
{
}

void
Mindriver::provide(const String& req, ErrorHandler* errh)
{
    if (verbose && _provisions[req] < 0)
	errh->message("providing '%s'", req.c_str());
    _provisions.insert(req, 1);
}

void
Mindriver::require(const String& req, ErrorHandler* errh)
{
    if (_provisions[req] < 0) {
	if (verbose && _requirements[req] < 0)
	    errh->message("requiring '%s'", req.c_str());
	_requirements.insert(req, 1);
    }
}

void
Mindriver::add_source_file(const String& fn, ErrorHandler* errh)
{
    if (verbose && _source_files[fn] < 0)
	errh->message("adding source file '%s'", fn.c_str());
    _source_files.insert(fn, 1);
}

void
Mindriver::add_router_requirements(RouterT* router, const ElementMap& default_map, ErrorHandler* errh)
{
    // find and parse elementmap
    ElementMap emap(default_map);
    emap.parse_requirement_files(router, CLICK_SHAREDIR, errh);

    // check whether suitable for driver
    if (!emap.driver_compatible(router, driver)) {
	errh->error("not compatible with %s driver; ignored", Driver::name(driver));
	return;
    }
    emap.set_driver(driver);

    StringAccum missing_sa;
    int nmissing = 0;

    HashMap<ElementClassT*, int> primitives(-1);
    router->collect_types(primitives);
    for (HashMap<ElementClassT*, int>::iterator i = primitives.begin(); i; i++) {
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
	errh->fatal("cannot locate required element class '%s'\n(This may be due to a missing or out-of-date 'elementmap.xml'.)", missing_sa.c_str());
    else if (nmissing > 1)
	errh->fatal("cannot locate these required element classes:\n  %s\n(This may be due to a missing or out-of-date 'elementmap.xml'.)", missing_sa.c_str());
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
	filename = "<expr>";
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
    
    if (_provisions[requirement] > 0)
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
	errh->error("cannot satisfy requirement '%s'", requirement.c_str());
    return false;
}

void
Mindriver::print_elements_conf(FILE *f, String package, const ElementMap &emap)
{
    Vector<String> sourcevec;
    for (HashMap<String, int>::iterator iter = _source_files.begin();
	 iter;
	 iter++) {
	iter.value() = sourcevec.size();
	sourcevec.push_back(iter.key());
    }

    Vector<String> headervec(sourcevec.size(), String());
    Vector<String> classvec(sourcevec.size(), String());

    // collect header file and C++ element class definitions from emap
    for (int i = 1; i < emap.size(); i++) {
	const Traits &elt = emap.traits_at(i);
	int sourcei = _source_files[elt.source_file];
	if (sourcei >= 0) {
	    headervec[sourcei] = elt.header_file;
	    if (elt.name)
		classvec[sourcei] += " " + elt.cxx + "-" + elt.name;
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
		fprintf(f, "%s\t\"%s\"\t%s\n", sourcevec[i].c_str(), headervec[i].c_str(), classstr.c_str());
	    else
		fprintf(f, "%s\t%s\t%s\n", sourcevec[i].c_str(), headervec[i].c_str(), classstr.c_str());
	}
}

static int
print_makefile(const String &directory, const String &pkg, const StringAccum &sa, ErrorHandler *errh)
{
    String fn = directory + "Makefile." + pkg;
    errh->message("Creating %s...", fn.c_str());
    FILE *f = fopen(fn.c_str(), "w");
    if (!f)
	return errh->error("%s: %s", fn.c_str(), strerror(errno));
  
    fwrite(sa.data(), 1, sa.length(), f);
  
    fclose(f);
    return 0;
}

static int
print_u_makefile(const String &directory, const String &pkg, bool check, ErrorHandler *errh)
{
    int before = errh->nerrors();

    if (check) {
	String fn = directory + "Makefile";
	String text = file_string(fn, errh);
	if (before != errh->nerrors())
	    return -1;

	String expectation = String("\n## Click ") + Driver::requirement(driver) + " driver Makefile ##\n";
	if (text.find_left(expectation) < 0)
	    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.c_str(), Driver::name(driver));
    }
  
    StringAccum sa;
    sa << "INSTALLPROGS = " << pkg << "click\n\
include Makefile\n\n";
    sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_builddir)/click-buildtool\n\
	$(top_builddir)/click-buildtool elem2make -x \"$(STD_ELEMENT_OBJS)\" -v ELEMENT_OBJS_" << pkg << " < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
elements_" << pkg << ".cc: elements_" << pkg << ".conf $(top_builddir)/click-buildtool\n\
	$(top_builddir)/click-buildtool elem2export < elements_" << pkg << ".conf > elements_" << pkg << ".cc\n\
	@rm -f elements_" << pkg << ".d\n";
    sa << "-include elements_" << pkg << ".mk\n";
    sa << "OBJS_" << pkg << " = $(ELEMENT_OBJS_" << pkg << ") elements_" << pkg << ".o click.o\n";
    sa << pkg << "click: Makefile Makefile." << pkg << " libclick.a $(OBJS_" << pkg << ")\n\
	$(CXXLINK) -rdynamic $(OBJS_" << pkg << ") $(LIBS) libclick.a\n";

    return print_makefile(directory, pkg, sa, errh);
}

static int
print_k_makefile(const String &directory, const String &pkg, bool check, ErrorHandler *errh)
{
    int before = errh->nerrors();

    if (check) {
	String fn = directory + "Makefile";
	String text = file_string(fn, errh);
	if (before != errh->nerrors())
	    return -1;

	String expectation = String("\n## Click ") + Driver::requirement(driver) + " driver Makefile ##\n";
	if (text.find_left(expectation) < 0)
	    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.c_str(), Driver::name(driver));
    }
  
    StringAccum sa;
    sa << "INSTALLOBJS = " << pkg << "click.o\n\
include Makefile\n\n";
    sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_builddir)/click-buildtool\n\
	$(top_builddir)/click-buildtool elem2make -x \"$(STD_ELEMENT_OBJS)\" -v ELEMENT_OBJS_" << pkg << " < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
elements_" << pkg << ".cc: elements_" << pkg << ".conf $(top_builddir)/click-buildtool\n\
	$(top_builddir)/click-buildtool elem2export < elements_" << pkg << ".conf > elements_" << pkg << ".cc\n\
	@rm -f elements_" << pkg << ".d\n";
    sa << "-include elements_" << pkg << ".mk\n";
    sa << "OBJS_" << pkg << " = $(GENERIC_OBJS) $(ELEMENT_OBJS_" << pkg << ") $(LINUXMODULE_OBJS) elements_" << pkg << ".o\n";
    sa << pkg << "click.o: Makefile Makefile." << pkg << " $(OBJS_" << pkg << ")\n\
	$(LD) -r -o " << pkg << "click.o $(OBJS_" << pkg << ")\n\
	$(STRIP) -g " << pkg << "click.o\n";

    return print_makefile(directory, pkg, sa, errh);
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
    bool need_file = true;
    bool check = true;

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
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;
      
	  case CLICKPATH_OPT:
	    set_clickpath(clp->arg);
	    break;

	  case KERNEL_OPT:
	    driver = Driver::LINUXMODULE;
	    break;
      
	  case USERLEVEL_OPT:
	    driver = Driver::USERLEVEL;
	    break;

	  case PACKAGE_OPT:
	    package_name = clp->arg;
	    break;

	  case DIRECTORY_OPT:
	    directory = clp->arg;
	    if (directory.length() && directory.back() != '/')
		directory += "/";
	    break;

	  case ALL_OPT:
	    specifier = (clp->negated ? "x" : "a");
	    break;

	  case ELEMENT_OPT: {
	      Vector<String> elements;
	      cp_spacevec(clp->arg, elements);
	      for (String *e = elements.begin(); e < elements.end(); e++)
		  md.require(*e, &arg_lerrh);
	      break;
	  }

	  case ALIGN_OPT:
	    md.require("Align", &arg_lerrh);
	    break; 

	  case CHECK_OPT:
	    check = !clp->negated;
	    break;

	  case VERBOSE_OPT:
	    verbose = !clp->negated;
	    break;

	  case ROUTER_OPT:
	  case Clp_NotOption:
	    if (clp->negated)
		need_file = false;
	    else
		router_filenames.push_back(specifier + String("f") + clp->arg);
	    break;

	  case EXPRESSION_OPT:
	    router_filenames.push_back(specifier + String("e") + clp->arg);
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
    if (!router_filenames.size() && need_file)
	router_filenames.push_back(specifier + "f-");
    if (!package_name)
	errh->fatal("fatal error: no package name specified\nPlease supply the '-p PKG' option.");

    ElementMap default_emap;
    if (!default_emap.parse_default_file(CLICK_SHAREDIR, errh))
	default_emap.report_file_not_found(CLICK_SHAREDIR, false, errh);
  
    for (int i = 0; i < router_filenames.size(); i++)
	handle_router(md, router_filenames[i], default_emap, errh);

    // add types that are always required
    {
	LandmarkErrorHandler lerrh(errh, "default requirements");
	md.require("AddressInfo", &lerrh);
	md.require("AlignmentInfo", &lerrh);
	md.require("DriverManager", &lerrh);
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
	HashMap<String, int> old_reqs(-1);
	old_reqs.swap(md._requirements);

	for (HashMap<String, int>::iterator iter = old_reqs.begin(); iter; iter++)
	    md.resolve_requirement(iter.key(), default_emap, errh);

	if (!md._requirements.size())
	    break;
    }

    // print files
    if (errh->nerrors() > 0)
	exit(1);

    // first, print Makefile.PKG
    if (driver == Driver::USERLEVEL)
	print_u_makefile(directory, package_name, check, errh);
    else if (driver == Driver::LINUXMODULE)
	print_k_makefile(directory, package_name, check, errh);
    else
	errh->fatal("%s driver support unimplemented", Driver::name(driver));

    // Then, print elements_PKG.conf
    if (errh->nerrors() == 0) {
	String fn = directory + String("elements_") + package_name + ".conf";
	errh->message("Creating %s...", fn.c_str());
	FILE *f = fopen(fn.c_str(), "w");
	if (!f)
	    errh->fatal("%s: %s", fn.c_str(), strerror(errno));
	md.print_elements_conf(f, package_name, default_emap);
	fclose(f);
    }

    // Final message
    if (errh->nerrors() == 0) {
	if (driver == Driver::USERLEVEL)
	    errh->message("Build '%sclick' with 'make -f Makefile.%s'.", package_name, package_name);
	else
	    errh->message("Build '%sclick.o' with 'make -f Makefile.%s'.", package_name, package_name);
	return 0;
    } else
	exit(1);
}
