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
#include <cstdio>
#include <cctype>
#include <ctime>

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

static Clp_Option options[] = {
  { "align", 'A', ALIGN_OPT, 0, 0 },
  { "all", 'a', ALL_OPT, 0, Clp_Negate },
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
};

static const char *program_name;
static String::Initializer string_initializer;

static int driver = -1;
static HashMap<String, int> initial_requirements(-1);

void
short_usage()
{
  fprintf(stderr, "Usage: %s -p PKGNAME [OPTION]... [ROUTERFILE]...\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-mkmindriver' produces a Makefile that builds a minimum Click driver\n\
for a set of router configurations. This driver contains just the elements\n\
those configurations require. Run `click-mkmindriver' in the relevant driver's\n\
build directory and supply a package name with the `-p PKG' option. The\n\
resulting Makefile is called `Makefile.PKG'; it will build either a `PKGclick'\n\
user-level driver or a `PKGclick.o' kernel module.\n\
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
  -d, --directory DIR      Put files in DIR. DIR must contain a `Makefile'\n\
                           for the relevant driver. Default is `.'.\n\
  -E, --elements ELTS      Include element classes ELTS.\n\
  -A, --align              Include element classes required by click-align.\n\
      --no-file            Don't read a configuration from standard input.\n\
  -C, --clickpath PATH     Use PATH for CLICKPATH.\n\
      --help               Print this message and exit.\n\
  -v, --version            Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
handle_router(String filename_in, const ElementMap &default_map, ErrorHandler *errh)
{
  // decide if `filename' should be flattened
  bool flattenable = (filename_in[0] != 'a');
  bool file_is_expr = (filename_in[1] == 'e');
  const char *filename = filename_in.cc() + 2;

  // read file
  int before = errh->nerrors();
  RouterT *router = read_router(filename, file_is_expr, errh);
  if (router && flattenable)
    router->flatten(errh);
  if (!router || errh->nerrors() != before)
    return;
  if (file_is_expr)
    filename = "<expr>";
  else if (!filename || strcmp(filename, "-") == 0)
    filename = "<stdin>";
  
  // find and parse elementmap
  ElementMap emap(default_map);
  emap.parse_requirement_files(router, CLICK_SHAREDIR, errh);

  // check whether suitable for driver
  if (!emap.driver_compatible(router, driver)) {
    errh->error("%s: not compatible with %s driver; ignored", filename, Driver::name(driver));
    return;
  }
  emap.set_driver(driver);

  StringAccum missing_sa;
  int nmissing = 0;

  HashMap<String, int> primitives(-1);
  router->collect_primitive_classes(primitives);
  for (HashMap<String, int>::iterator i = primitives.begin(); i; i++) {
    if (!emap.has_traits(i.key()))
      missing_sa << (nmissing++ ? ", " : "") << i.key();
    else if (emap.package(i.key()))
      /* do nothing; element was defined in a package */;
    else
      initial_requirements.insert(i.key(), 1);
  }

  if (nmissing == 1)
    errh->fatal("%s: cannot locate required element class `%s'\n(This may be due to a missing or out-of-date elementmap.)", filename, missing_sa.cc());
  else if (nmissing > 1)
    errh->fatal("%s: cannot locate these required element classes:\n  %s\n(This may be due to a missing or out-of-date elementmap.)", filename, missing_sa.cc());
}

static void
add_stuff(int ti, const ElementMap &emap,
	  HashMap<String, int> &provisions, Vector<String> &requirements,
	  HashMap<String, int> &source_files)
{
  const Traits &t = emap.traits_at(ti);
  
  if (t.provisions) {
    Vector<String> args;
    cp_spacevec(t.provisions, args);
    for (int j = 0; j < args.size(); j++)
      provisions.insert(args[j], 1);
  }
  if (t.name)
    provisions.insert(t.name, 1);

  if (t.requirements) {
    Vector<String> args;
    cp_spacevec(t.requirements, args);
    for (int j = 0; j < args.size(); j++)
      requirements.push_back(args[j]);
  }

  if (t.source_file)
    source_files.insert(t.source_file, 1);
}

static void
find_requirement(const String &requirement, const ElementMap &emap,
		 Vector<int> &new_emapi, ErrorHandler *errh)
{
  int try_name_emapi = emap.traits_index(requirement);
  if (try_name_emapi > 0) {
    new_emapi.push_back(try_name_emapi);
    return;
  }
  
  for (int i = 1; i < emap.size(); i++)
    if (emap.traits_at(i).provides(requirement)) {
      new_emapi.push_back(i);
      return;
    }

  errh->error("cannot satisfy requirement `%s' from default elementmap", String(requirement).cc());
}

static void
print_elements_conf(FILE *f, String package, const ElementMap &emap,
		    HashMap<String, int> &source_files)
{
  Vector<String> sourcevec;
  for (HashMap<String, int>::iterator iter = source_files.begin();
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
    int sourcei = source_files[elt.source_file];
    if (sourcei >= 0) {
      headervec[sourcei] = elt.header_file;
      if (elt.name)
	classvec[sourcei] += " " + elt.cxx;
    }
  }

  // output data
  time_t now = time(0);
  const char *date_str = ctime(&now);
  fprintf(f, "# Generated by 'click-mkmindriver -p %s' on %s", package.cc(), date_str);
  for (int i = 0; i < sourcevec.size(); i++)
    if (headervec[i]) {
      if (headervec[i][0] != '\"' && headervec[i][0] != '<')
	fprintf(f, "%s \"%s\"%s\n", sourcevec[i].cc(), headervec[i].cc(), classvec[i].cc());
      else
	fprintf(f, "%s %s%s\n", sourcevec[i].cc(), headervec[i].cc(), classvec[i].cc());
    }
}

static int
print_makefile(const String &directory, const String &pkg, const StringAccum &sa, ErrorHandler *errh)
{
  String fn = directory + "Makefile." + pkg;
  errh->message("Creating %s...", fn.cc());
  FILE *f = fopen(fn.cc(), "w");
  if (!f)
    return errh->error("%s: %s", fn.cc(), strerror(errno));
  
  fwrite(sa.data(), 1, sa.length(), f);
  
  fclose(f);
  return 0;
}

static int
print_u_makefile(const String &directory, const String &pkg, ErrorHandler *errh)
{
  int before = errh->nerrors();
  String fn = directory + "Makefile";
  String text = file_string(fn, errh);
  if (before != errh->nerrors())
    return -1;

  String expectation = String("\n## Click ") + Driver::requirement(driver) + " driver Makefile ##\n";
  if (text.find_left(expectation) < 0)
    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.cc(), Driver::name(driver));
  
  StringAccum sa;
  sa << "INSTALLPROGS = " << pkg << "click\n\
include Makefile\n\n";
  sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2make -x addressinfo.o -x alignmentinfo.o -x errorelement.o -x scheduleinfo.o -x drivermanager.o -v ELEMENT_OBJS_" << pkg << ") < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
elements_" << pkg << ".cc: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2export) < elements_" << pkg << ".conf > elements_" << pkg << ".cc\n\
	@rm -f elements_" << pkg << ".d\n";
  sa << "-include elements_" << pkg << ".mk\n";
  sa << "OBJS_" << pkg << " = $(ELEMENT_OBJS_" << pkg << ") elements_" << pkg << ".o click.o\n";
  sa << pkg << "click: Makefile Makefile." << pkg << " libclick.a $(OBJS_" << pkg << ")\n\
	$(CXXLINK) -rdynamic $(OBJS_" << pkg << ") $(LIBS) libclick.a\n";

  return print_makefile(directory, pkg, sa, errh);
}

static int
print_k_makefile(const String &directory, const String &pkg, ErrorHandler *errh)
{
  int before = errh->nerrors();
  String fn = directory + "Makefile";
  String text = file_string(fn, errh);
  if (before != errh->nerrors())
    return -1;

  String expectation = String("\n## Click ") + Driver::requirement(driver) + " driver Makefile ##\n";
  if (text.find_left(expectation) < 0)
    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.cc(), Driver::name(driver));
  
  StringAccum sa;
  sa << "INSTALLOBJS = " << pkg << "click.o\n\
include Makefile\n\n";
  sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2make -x addressinfo.o -x alignmentinfo.o -x errorelement.o -x scheduleinfo.o -x drivermanager.o -v ELEMENT_OBJS_" << pkg << ") < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
elements_" << pkg << ".cc: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2export) < elements_" << pkg << ".conf > elements_" << pkg << ".cc\n\
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
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  Vector<String> router_filenames;
  String specifier = "x";
  Vector<String> elements;
  const char *package_name = 0;
  String directory;
  bool need_file = true;
  
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

     case ELEMENT_OPT:
      cp_spacevec(clp->arg, elements);
      break;

     case ALIGN_OPT:
      elements.push_back("Align");
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
    errh->fatal("fatal error: no package name specified\nPlease supply the `-p PKG' option.");

  ElementMap default_emap;
  if (!default_emap.parse_default_file(CLICK_SHAREDIR, errh))
    default_emap.report_file_not_found(CLICK_SHAREDIR, false, errh);
  
  for (int i = 0; i < router_filenames.size(); i++)
    handle_router(router_filenames[i], default_emap, errh);

  // add types that are always required
  initial_requirements.insert("AddressInfo", 1);
  initial_requirements.insert("AlignmentInfo", 1);
  initial_requirements.insert("Error", 1);
  initial_requirements.insert("ScheduleInfo", 1);
  initial_requirements.insert("DriverManager", 1);
  if (driver == Driver::USERLEVEL) {
    initial_requirements.insert("QuitWatcher", 1);
    initial_requirements.insert("ControlSocket", 1);
  }

  // add manually specified element classes
  for (int i = 0; i < elements.size(); i++)
    initial_requirements.insert(elements[i], 1);
  
  HashMap<String, int> provisions(-1);
  Vector<String> requirements;
  HashMap<String, int> source_files(-1);

  // add initial provisions
  default_emap.set_driver(driver);
  provisions.insert(Driver::requirement(driver), 1);
  // all default provisions are stored in elementmap index 0
  add_stuff(0, default_emap, provisions, requirements, source_files);
  
  // process initial requirements and provisions
  for (HashMap<String, int>::iterator iter = initial_requirements.begin();
       iter;
       iter++) {
    int emapi = default_emap.traits_index(iter.key());
    if (emapi > 0)
      add_stuff(emapi, default_emap, provisions, requirements, source_files);
  }

  // now, loop over requirements until closure
  while (1) {
    Vector<int> new_emapi;
    for (int i = 0; i < requirements.size(); i++)
      if (provisions[ requirements[i] ] > 0)
	/* OK; already have provision */;
      else
	find_requirement(requirements[i], default_emap, new_emapi, errh);

    if (!new_emapi.size())
      break;

    requirements.clear();
    for (int i = 0; i < new_emapi.size(); i++)
      add_stuff(new_emapi[i], default_emap, provisions, requirements, source_files);
  }

  // print files
  if (errh->nerrors() > 0)
    exit(1);

  // first, print Makefile.PKG
  if (driver == Driver::USERLEVEL)
    print_u_makefile(directory, package_name, errh);
  else if (driver == Driver::LINUXMODULE)
    print_k_makefile(directory, package_name, errh);
  else
    errh->fatal("%s driver support unimplemented", Driver::name(driver));

  // Then, print elements_PKG.conf
  if (errh->nerrors() == 0) {
    String fn = directory + String("elements_") + package_name + ".conf";
    errh->message("Creating %s...", fn.cc());
    FILE *f = fopen(fn.cc(), "w");
    if (!f)
      errh->fatal("%s: %s", fn.cc(), strerror(errno));
    print_elements_conf(f, package_name, default_emap, source_files);
    fclose(f);
  }

  // Final message
  if (errh->nerrors() == 0) {
    if (driver == Driver::USERLEVEL)
      errh->message("Build `%sclick' with `make -f Makefile.%s'.", package_name, package_name);
    else
      errh->message("Build `%sclick.o' with `make -f Makefile.%s'.", package_name, package_name);
    return 0;
  } else
    exit(1);
}
