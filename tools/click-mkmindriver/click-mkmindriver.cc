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
#define ROUTER_OPT		301

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
};

static const char *program_name;

static String::Initializer initialize_strings;
static HashMap<String, int> initial_requirements(-1);

void
short_usage()
{
  fprintf(stderr, "Usage: %s ROUTERFILE\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-shrink' produces an userlevel build environment for a specific router\n\
configuration. The resulting Makefile will build a click executable with\n\
only libclick.a and elements used by the router file.\n\
\n\
Usage: %s ROUTERFILE\n\
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
  emap.limit_driver(ElementMap::DRIVER_USERLEVEL); // XXX
  
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
add_provisions_and_requirements(int new_emapi, const ElementMap &emap,
				HashMap<String, int> &provisions,
				Vector<String> &requirements)
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
}

static void
add_filename(int new_emapi, const ElementMap &emap,
	     HashMap<String, int> &filenames)
{
  const String &fn = emap.header_file(new_emapi);
  if (fn.substring(-3) == ".hh")
    filenames.insert(fn.substring(0, -3) + ".cc", 1);
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
  
  for (int i = 1; i < emap.size(); i++) {
    const String &prov = emap.provisions(i);
    if (prov) {
      int where = prov.find_left(requirement);
      if (where >= 0) {
	// be careful about finding the requirement as part of a substring
	int rwhere = where + requirement.length();
	if ((where == 0 || isspace(prov[where - 1]))
	    && (rwhere == prov.length() || isspace(prov[rwhere]))) {
	  // found it!
	  new_emapi.push_back(i);
	  return;
	}
      }
    }
  }

  errh->error("cannot satisfy requirement `%s' from default elementmap", String(requirement).cc());
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
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
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
  if (!router_filenames.size())
    router_filenames.push_back("-");

  ElementMap default_emap;
  default_emap.parse_default_file(CLICK_SHAREDIR, errh);
  
  for (int i = 0; i < router_filenames.size(); i++)
    handle_router(router_filenames[i], default_emap, errh);

  initial_requirements.insert("QuitWatcher", 1);
  initial_requirements.insert("ControlSocket", 1);
  
  HashMap<String, int> provisions(-1);
  Vector<String> requirements;
  HashMap<String, int> filenames(-1);

  default_emap.limit_driver(ElementMap::DRIVER_USERLEVEL); // XXX
  provisions.insert("userlevel", 1);
  
  // process initial requirements and provisions
  for (HashMap<String, int>::Iterator iter = initial_requirements.first();
       iter;
       iter++) {
    int emapi = default_emap.find(iter.key());
    add_provisions_and_requirements(emapi, default_emap, provisions, requirements);
    add_filename(emapi, default_emap, filenames);
  }

  // now, loop over requirements until you reach closure
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
    for (int i = 0; i < new_emapi.size(); i++) {
      add_provisions_and_requirements(new_emapi[i], default_emap, provisions, requirements);
      add_filename(new_emapi[i], default_emap, filenames);
    }
  }

  // print out filenames
  for (HashMap<String, int>::Iterator iter = filenames.first();
       iter;
       iter++)
    printf("%s\n", String(iter.key()).cc());

  return 0;
}
