/*
 * click-uncombine.cc -- separate one Click configuration from a combined
 * configuration
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "elementmap.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/driver.hh>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define OUTPUT_OPT		303
#define NAME_OPT		304

static const Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "name", 'n', NAME_OPT, Clp_ValString, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String runclick_prog;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try '%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
'Click-uncombine' reads a combined Click configuration produced by\n\
click-combine and writes one of its components to the standard output.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE [COMPONENTNAME]]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -n, --name NAME             Output the router component named NAME.\n\
  -o, --output FILE           Write output to FILE.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
}

static Vector<ElementT *> component_endpoints;

static void
remove_component_links(RouterT *r, ErrorHandler *errh, const String &component)
{
  // prepare
  ElementClassT *link_type = ElementClassT::base_type("RouterLink");
  if (!link_type)
    return;
  component_endpoints.clear();

  // find all links
  Vector<ElementT *> links;
  for (RouterT::type_iterator x = r->begin_elements(link_type); x; x++)
    links.push_back(x.get());

  for (int i = 0; i < links.size(); i++) {
    // check configuration string for correctness
    String link_name = links[i]->name();
    Vector<String> words;
    cp_argvec(links[i]->configuration(), words);
    int ninputs = links[i]->ninputs();
    int noutputs = links[i]->noutputs();
    if (words.size() != 2 * (ninputs + noutputs) || !ninputs || !noutputs) {
      errh->error("RouterLink '%s' has strange configuration", link_name.c_str());
      continue;
    }

    // check if this RouterLink involves the interesting component
    bool interesting = false, bad = false, subcomponent = false;
    for (int j = 0; !bad && j < words.size(); j += 2) {
      Vector<String> clauses;
      cp_spacevec(words[j], clauses);
      if (clauses.size() != 3) {
	errh->error("RouterLink '%s' has strange configuration", link_name.c_str());
	bad = true;
      } else if (clauses[0] == component)
	interesting = true;
      else if (clauses[0].substring(0, component.length()) == component
	       && clauses[0][component.length()] == '/')
	subcomponent = true;
    }
    if (subcomponent && !bad && !interesting)
      // save as part of this component
      component_endpoints.push_back(links[i]);
    if (bad || !interesting)
      continue;

    // separate out this component
    for (int j = 0; j < words.size(); j += 2) {
      Vector<String> clauses;
      cp_spacevec(words[j], clauses);
      String name = clauses[0] + "/" + clauses[1];
      if (ElementT *preexist = r->element(name)) {
	errh->lerror(links[i]->landmark(), "RouterLink '%s' element '%s' already exists", link_name.c_str(), name.c_str());
	errh->lerror(preexist->landmark(), "(previous definition was here)");
      } else if (clauses[0] == component) {
	ElementT *newe = r->get_element(clauses[1], ElementClassT::base_type(clauses[2]), words[j+1], LandmarkT("<click-uncombine>"));
	if (j/2 < ninputs)
	  r->insert_before(newe, PortT(links[i], j/2));
	else
	  r->insert_after(newe, PortT(links[i], j/2 - ninputs));
	component_endpoints.push_back(newe);
      }
    }

    // remove link
    r->free_element(links[i]);
  }
}

static void
mark_component(RouterT *r, String compname, Vector<int> &live)
{
  assert(compname.back() == '/');

  // mark endpoints
  for (int i = 0; i < component_endpoints.size(); i++)
    live[component_endpoints[i]->eindex()] = 1;

  // mark everything named with a 'compname' prefix
  int compname_len = compname.length();
  for (RouterT::iterator e = r->begin_elements(); e; e++)
    if (e->name().substring(0, compname_len) == compname)
      live[e->eindex()] = 1;

  // now find things connected to live elements
  bool changed;
  do {
    changed = false;
    for (RouterT::conn_iterator it = r->begin_connections();
	 it != r->end_connections(); ++it) {
	if (live[it->from_eindex()] && !live[it->to_eindex()])
	    live[it->to_eindex()] = changed = true;
	else if (live[it->to_eindex()] && !live[it->from_eindex()])
	    live[it->from_eindex()] = changed = true;
    }
  } while (changed);

  // print names of lives
  //for (int i = 0; i < ne; i++)
  //if (live[i])
  //fprintf(stderr, "%s\n", r->ename(i).c_str());
}

static void
frob_nested_routerlinks(RouterT *r, const String &compname)
{
  ElementClassT *t = ElementClassT::base_type("RouterLink");
  int cnamelen = compname.length();
  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    Vector<String> words;
    cp_argvec(x->configuration(), words);
    for (int j = 0; j < words.size(); j += 2) {
      if (words[j].substring(0, cnamelen) == compname)
	words[j] = words[j].substring(cnamelen);
    }
    x->set_configuration(cp_unargvec(words));
  }
}

static void
remove_toplevel_component(String component, RouterT *r, const char *filename,
			  ErrorHandler *errh, const String &component_prefix)
{
  // find component names
  HashTable<String, int> component_map(-1);
  Vector<String> component_names;
  if (r->archive_index("componentmap") >= 0) {
    ArchiveElement &ae = r->archive("componentmap");
    cp_spacevec(ae.data, component_names);
    for (int i = 0; i < component_names.size(); i++)
      component_map.set(component_names[i], 0);
  }

  // check if component exists
  if (component_map.get(component) < 0) {
    String g = component_prefix + component;
    errh->fatal("%s: no '%s' component", filename, g.c_str());
  }

  // remove top-level links
  remove_component_links(r, errh, component);

  // mark everything connected to the endpoints
  component += "/";
  Vector<int> live(r->nelements(), 0);
  mark_component(r, component, live);

  // remove everything not part of the component
  for (RouterT::iterator e = r->begin_elements(); e; e++)
    if (e->live() && !live[e->eindex()])
      e->kill();
  r->free_dead_elements();

  // rename component
  int cnamelen = component.length();
  for (int i = 0; i < r->nelements(); i++)
    if (live[i]) {
      String name = r->element(i)->name();
      if (name.substring(0, cnamelen) == component
	  && r->eindex(name.substring(cnamelen)) < 0)
	r->change_ename(i, name.substring(cnamelen));
    }

  // fix nested RouterLinks
  frob_nested_routerlinks(r, component);

  // exit if there have been errors
  r->flatten(errh);
  if (errh->nerrors() != 0)
    exit(1);

  // update or remove componentmap
  {
    ArchiveElement &ae = r->archive("componentmap");
    StringAccum sa;
    int len = component.length();
    for (int i = 0; i < component_names.size(); i++)
      if (component_names[i].substring(0, len) == component)
	sa << component_names[i].substring(len) << '\n';
    if (sa.length())
      ae.data = sa.take_string();
    else
      ae.kill();
  }
}

int
main(int argc, char **argv)
{
  click_static_initialize();
  CLICK_DEFAULT_PROVIDES;
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-uncombine: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  String component;

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

     case HELP_OPT:
      usage();
      exit(0);
      break;

     case VERSION_OPT:
      printf("click-uncombine (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case ROUTER_OPT:
      if (router_file) {
	p_errh->error("combined router specified twice");
	goto bad_option;
      }
      router_file = clp->vstr;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->vstr;
      break;

     case NAME_OPT:
      if (component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      }
      component = clp->vstr;
      break;

     case Clp_NotOption:
      // if only one argument given, it's a component name
      if (router_file && component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      } else if (router_file)
	component = clp->vstr;
      else
	router_file = clp->vstr;
      break;

     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;

     case Clp_Done:
      goto done;

    }
  }

 done:
  RouterT *r = read_router_file(router_file, errh);
  if (r)
    r->flatten(errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";

  // find component names
  if (r->archive_index("componentmap") < 0)
    errh->fatal("%s: not created by 'click-combine' (no 'componentmap')", router_file);
  else if (!component)
    p_errh->fatal("no component specified");

  // walk down one slash at a time
  String prefix;
  while (component) {
    String toplevel = component.substring(component.begin(), find(component, '/'));
    String suffix = component.substring(toplevel.end() + 1, component.end());
    remove_toplevel_component(toplevel, r, router_file, errh, prefix);
    component = suffix;
    prefix += component + "/";
  }

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  write_router_file(r, outf, errh);
  exit(0);
}
