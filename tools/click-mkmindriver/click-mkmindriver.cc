#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include <click/clp.h>
#include <stdio.h>
#include <ctype.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define PACKAGE_OPT		303
#define DIRECTORY_OPT		304
#define KERNEL_OPT		305
#define USERLEVEL_OPT		306

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "directory", 'd', DIRECTORY_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, 0 },
  { "package", 'p', PACKAGE_OPT, Clp_ArgString, 0 },
  { "user", 'u', USERLEVEL_OPT, 0, 0 },
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
`Click-mkmindriver' produces a build environment for a specific router\n\
configuration. The resulting Makefile will build a `PKGclick' executable, or\n\
`PKGclick.o' kernel module, containing only the elements required by the\n\
router configuration or configurations.\n\
\n\
Usage: %s -p PKGNAME [OPTION]... [ROUTERFILE]...\n\
\n\
Options:\n\
  -f, --file FILE           Read router configuration from FILE.\n\
  -p, --package PKG         Name of package is PKGN. User-level driver will be\n\
                            called `PKGclick'; kernel module `PKGclick.o'.\n\
  -k, --kernel              Check kernel driver version of configuration.\n\
  -u, --user                Check user-level driver version of configuration.\n\
  -d, --directory DIR       Change build environment located in DIR.\n\
      --help                Print this message and exit.\n\
  -v, --version             Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static void
handle_router(const char *filename, const ElementMap &default_map, ErrorHandler *errh)
{
  int before = errh->nerrors();
  RouterT *router = read_router_file(filename, new RouterT, errh);
  if (router)
    router->flatten(errh);
  if (!router || errh->nerrors() != before)
    return;
  if (!filename || strcmp(filename, "-") == 0)
    filename = "<stdin>";
  
  // find and parse `elementmap'
  ElementMap emap(default_map);
  emap.parse_requirement_files(router, CLICK_SHAREDIR, errh);

  // check whether suitable for driver
  if (!emap.driver_compatible(router, driver, errh)) {
    const char *other_option = (driver == ElementMap::DRIVER_USERLEVEL ? "--kernel" : "--user");
    errh->error("%s: not compatible with %s driver; ignored\n%s: (Supply the `%s' option to include this router.)", filename, ElementMap::driver_name(driver), filename, other_option);
    return;
  }
  emap.limit_driver(driver);
  
  for (int i = RouterT::FIRST_REAL_TYPE; i < router->ntypes(); i++) {
    String tname = router->type_name(i);
    int emap_index = emap.find(tname);
    if (emap_index > 0) {
      if (emap.package(emap_index))
	/* do nothing; element was defined in a package */;
      else
	initial_requirements.insert(tname, 1);
    } else
      errh->warning("%s: cannot find required element class `%s'", filename, tname.cc());
  }
}

static void
add_stuff(int new_emapi, const ElementMap &emap,
	  HashMap<String, int> &provisions, Vector<String> &requirements,
	  HashMap<String, int> &header_files)
{
  const String &prov = emap.provisions(new_emapi);
  if (prov) {
    Vector<String> args;
    cp_spacevec(prov, args);
    for (int j = 0; j < args.size(); j++)
      provisions.insert(args[j], 1);
  }
  const String &click_name = emap.name(new_emapi);
  if (click_name)
    provisions.insert(click_name, 1);

  const String &req = emap.requirements(new_emapi);
  if (req) {
    Vector<String> args;
    cp_spacevec(req, args);
    for (int j = 0; j < args.size(); j++)
      requirements.push_back(args[j]);
  }

  const String &fn = emap.header_file(new_emapi);
  if (fn)
    header_files.insert(fn, 1);
}

static void
find_requirement(const String &requirement, const ElementMap &emap,
		 Vector<int> &new_emapi, ErrorHandler *errh)
{
  int try_name_emapi = emap.find(requirement);
  if (try_name_emapi > 0) {
    new_emapi.push_back(try_name_emapi);
    return;
  }
  
  for (int i = 1; i < emap.size(); i++)
    if (emap.provides(i, requirement)) {
      new_emapi.push_back(i);
      return;
    }

  errh->error("cannot satisfy requirement `%s' from default elementmap", String(requirement).cc());
}

static void
print_filenames(FILE *f, const HashMap<String, int> &header_files)
{
  for (HashMap<String, int>::Iterator iter = header_files.first();
       iter;
       iter++) {
    const String &fn = iter.key();
    if (fn.substring(-3) == ".hh")
      fprintf(f, "%.*s.cc\n", fn.length() - 3, fn.data());
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

  String expectation = String("\n## Click ") + ElementMap::driver_requirement(driver) + " driver Makefile ##\n";
  if (text.find_left(expectation) < 0)
    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.cc(), ElementMap::driver_name(driver));
  
  StringAccum sa;
  sa << "INSTALLPROGS = " << pkg << "click\n\
include Makefile\n\n";
  sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2make -x addressinfo.o -x alignmentinfo.o -x errorelement.o -x scheduleinfo.o -v ELEMENT_OBJS_" << pkg << ") < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
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

  String expectation = String("\n## Click ") + ElementMap::driver_requirement(driver) + " driver Makefile ##\n";
  if (text.find_left(expectation) < 0)
    return errh->error("%s does not contain magic string\n(Does this directory have a Makefile for Click's %s driver?)", fn.cc(), ElementMap::driver_name(driver));
  
  StringAccum sa;
  sa << "INSTALLOBJS = " << pkg << "click.o\n\
include Makefile\n\n";
  sa << "elements_" << pkg << ".mk: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2make -v ELEMENT_OBJS_" << pkg << ") < elements_" << pkg << ".conf > elements_" << pkg << ".mk\n\
elements_" << pkg << ".cc: elements_" << pkg << ".conf $(top_srcdir)/click-buildtool\n\
	(cd $(top_srcdir); ./click-buildtool elem2export) < elements_" << pkg << ".conf > elements_" << pkg << ".cc\n\
	@rm -f elements_" << pkg << ".d\n";
  sa << "-include elements_" << pkg << ".mk\n";
  sa << "OBJS_" << pkg << " = $(GENERIC_OBJS) $(ELEMENT_OBJS_" << pkg << ") $(LINUXMODULE_OBJS) elements_" << pkg << ".o\n";
  sa << pkg << "click.o: Makefile Makefile." << pkg << " $(OBJS_" << pkg << ")\n\
	ld -r -o " << pkg << "click.o $(OBJS_" << pkg << ")\n\
	strip -g " << pkg << "click.o\n";

  return print_makefile(directory, pkg, sa, errh);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  Vector<String> router_filenames;
  const char *package_name = 0;
  String directory;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case VERSION_OPT:
      printf("click-mkmindriver (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2001 Massachusetts Institute of Technology\n\
Copyright (c) 2001 ACIRI\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;

     case KERNEL_OPT:
      driver = ElementMap::DRIVER_LINUXMODULE;
      break;
      
     case USERLEVEL_OPT:
      driver = ElementMap::DRIVER_USERLEVEL;
      break;

     case PACKAGE_OPT:
      package_name = clp->arg;
      break;

     case DIRECTORY_OPT:
      directory = clp->arg;
      if (directory.length() && directory.back() != '/')
	directory += "/";
      break;
      
     case Clp_NotOption:
     case ROUTER_OPT:
      router_filenames.push_back(clp->arg);
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
    driver = ElementMap::DRIVER_USERLEVEL;
  if (!router_filenames.size())
    router_filenames.push_back("-");
  if (!package_name)
    errh->fatal("fatal error: no package name specified\nPlease supply the `-p PKG' option.");

  ElementMap default_emap;
  default_emap.parse_default_file(CLICK_SHAREDIR, errh);
  
  for (int i = 0; i < router_filenames.size(); i++)
    handle_router(router_filenames[i], default_emap, errh);

  // add types that are always required
  initial_requirements.insert("AddressInfo", 1);
  initial_requirements.insert("AlignmentInfo", 1);
  initial_requirements.insert("Error", 1);
  initial_requirements.insert("ScheduleInfo", 1);
  if (driver == ElementMap::DRIVER_USERLEVEL) {
    initial_requirements.insert("QuitWatcher", 1);
    initial_requirements.insert("ControlSocket", 1);
  }
  
  HashMap<String, int> provisions(-1);
  Vector<String> requirements;
  HashMap<String, int> header_files(-1);

  default_emap.limit_driver(driver);
  provisions.insert(ElementMap::driver_requirement(driver), 1);
  
  // process initial requirements and provisions
  for (HashMap<String, int>::Iterator iter = initial_requirements.first();
       iter;
       iter++) {
    int emapi = default_emap.find(iter.key());
    add_stuff(emapi, default_emap, provisions, requirements, header_files);
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
      add_stuff(new_emapi[i], default_emap, provisions, requirements, header_files);
  }

  // print files
  if (errh->nerrors() > 0)
    exit(1);

  // first, print Makefile.PKG
  if (driver == ElementMap::DRIVER_USERLEVEL)
    print_u_makefile(directory, package_name, errh);
  else if (driver == ElementMap::DRIVER_LINUXMODULE)
    print_k_makefile(directory, package_name, errh);
  else
    errh->fatal("%s driver support unimplemented", ElementMap::driver_name(driver));

  // Then, print elements_PKG.conf
  if (errh->nerrors() == 0) {
    String fn = directory + String("elements_") + package_name + ".conf";
    errh->message("Creating %s...", fn.cc());
    FILE *f = fopen(fn.cc(), "w");
    if (!f)
      errh->fatal("%s: %s", fn.cc(), strerror(errno));
    print_filenames(f, header_files);
    fclose(f);
  }

  // Final message
  if (errh->nerrors() == 0) {
    if (driver == ElementMap::DRIVER_USERLEVEL)
      errh->message("Build `%sclick' with `make -f Makefile.%s'.", package_name, package_name);
    else
      errh->message("Build `%sclick.o' with `make -f Makefile.%s'.", package_name, package_name);
    return 0;
  } else
    exit(1);
}
