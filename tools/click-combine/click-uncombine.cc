/*
 * click-uncombine.cc -- separate one Click configuration from a combined
 * configuration
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
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

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "name", 'n', NAME_OPT, Clp_ArgString, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String::Initializer string_initializer;
static String runclick_prog;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-uncombine' reads a combined Click configuration produced by\n\
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
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Vector<int> component_endpoints;

static void
remove_component_links(RouterT *r, ErrorHandler *errh, const String &component)
{
  // prepare
  int link_type = r->type_index("RouterLink");
  if (link_type < 0)
    return;
  component_endpoints.clear();

  // find all links
  Vector<int> links;
  for (int i = 0; i < r->nelements(); i++)
    if (r->etype(i) == link_type)
      links.push_back(i);

  for (int i = 0; i < links.size(); i++) {
    // check configuration string for correctness
    String link_name = r->ename(links[i]);
    Vector<String> words;
    cp_argvec(r->econfiguration(links[i]), words);
    int ninputs = r->ninputs(links[i]);
    int noutputs = r->noutputs(links[i]);
    if (words.size() != 2 * (ninputs + noutputs) || !ninputs || !noutputs) {
      errh->error("RouterLink `%s' has strange configuration", link_name.cc());
      continue;
    }
    
    // check if this RouterLink involves the interesting component
    bool interesting = false, bad = false, subcomponent = false;
    for (int j = 0; !bad && j < words.size(); j += 2) {
      Vector<String> clauses;
      cp_spacevec(words[j], clauses);
      if (clauses.size() != 3) {
	errh->error("RouterLink `%s' has strange configuration", link_name.cc());
	bad = true;
      } else if (clauses[0] == component)
	interesting = true;
      else if (clauses[0].substring(0, component.length()) == component
	       && clauses[0][component.length()] == '/')
	subcomponent = true;
    }
    if (subcomponent && !bad && !interesting)
      component_endpoints.push_back(i);	// save as part of this component
    if (bad || !interesting)
      continue;

    // separate out this component
    for (int j = 0; j < words.size(); j += 2) {
      Vector<String> clauses;
      cp_spacevec(words[j], clauses);
      String name = clauses[0] + "/" + clauses[1];
      if (r->eindex(name) >= 0) {
	errh->lerror(r->elandmark(links[i]), "RouterLink `%s' element `%s' already exists", link_name.cc(), name.cc());
	errh->lerror(r->elandmark(r->eindex(name)), "(previous definition was here)");
      } else if (clauses[0] == component) {
	int newe = r->get_eindex(clauses[1], r->get_type_index(clauses[2]), words[j+1], "<click-uncombine>");
	if (j/2 < ninputs)
	  r->insert_before(newe, Hookup(links[i], j/2));
	else
	  r->insert_after(newe, Hookup(links[i], j/2 - ninputs));
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
  
  int ne = r->nelements();
  int nh = r->nhookup();
  const Vector<Hookup> &hf = r->hookup_from();
  const Vector<Hookup> &ht = r->hookup_to();
  
  // mark endpoints
  for (int i = 0; i < component_endpoints.size(); i++)
    live[component_endpoints[i]] = 1;

  // mark everything named with a `compname' prefix
  int compname_len = compname.length();
  for (int i = 0; i < ne; i++)
    if (r->ename(i).substring(0, compname_len) == compname)
      live[i] = 1;

  // now find things connected to live elements
  bool changed;
  do {
    changed = false;
    for (int i = 0; i < nh; i++)
      if (hf[i].dead())
	/* nada */;
      else if (live[hf[i].idx] && !live[ht[i].idx]) {
	live[ht[i].idx] = 1;
	changed = true;
      } else if (live[ht[i].idx] && !live[hf[i].idx]) {
	live[hf[i].idx] = 1;
	changed = true;
      }
  } while (changed);

  // print names of lives
  //for (int i = 0; i < ne; i++)
  //if (live[i])
  //fprintf(stderr, "%s\n", r->ename(i).cc());
}

static void
frob_nested_routerlinks(RouterT *r, const String &compname)
{
  int ne = r->nelements();
  int t = r->get_type_index("RouterLink");
  int cnamelen = compname.length();
  for (int i = 0; i < ne; i++)
    if (r->etype(i) == t) {
      Vector<String> words;
      cp_argvec(r->econfiguration(i), words);
      for (int j = 0; j < words.size(); j += 2) {
	if (words[j].substring(0, cnamelen) == compname)
	  words[j] = words[j].substring(cnamelen);
      }
      r->econfiguration(i) = cp_unargvec(words);
    }
}

static void
remove_toplevel_component(String component, RouterT *r, const char *filename,
			  ErrorHandler *errh, const String &component_prefix)
{
  // find component names
  HashMap<String, int> component_map(-1);
  Vector<String> component_names;
  if (r->archive_index("componentmap") >= 0) {
    ArchiveElement &ae = r->archive("componentmap");
    cp_spacevec(ae.data, component_names);
    for (int i = 0; i < component_names.size(); i++)
      component_map.insert(component_names[i], 0);
  }

  // check if component exists
  if (component_map[component] < 0) {
    String g = component_prefix + component;
    errh->fatal("%s: no `%s' component", filename, g.cc());
  }

  // remove top-level links
  remove_component_links(r, errh, component);

  // mark everything connected to the endpoints
  component += "/";
  Vector<int> live(r->nelements(), 0);
  mark_component(r, component, live);
  
  // remove everything not part of the component
  for (int i = 0; i < r->nelements(); i++)
    if (!live[i] && r->elive(i))
      r->kill_element(i);
  r->free_dead_elements();

  // rename component
  int cnamelen = component.length();
  for (int i = 0; i < r->nelements(); i++)
    if (live[i]) {
      String name = r->ename(i);
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
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
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
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case NAME_OPT:
      if (component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      }
      component = clp->arg;
      break;

     case Clp_NotOption:
      // if only one argument given, it's a component name
      if (router_file && component) {
	p_errh->error("component name specified twice");
	goto bad_option;
      } else if (router_file)
	component = clp->arg;
      else
	router_file = clp->arg;
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
    errh->fatal("%s: not created by `click-combine' (no `componentmap')", router_file);
  else if (!component)
    p_errh->fatal("no component specified");
  
  // walk down one slash at a time
  String prefix;
  while (component) {
    int p = component.find_left('/');
    if (p < 0) p = component.length();
    String toplevel = component.substring(0, p);
    String suffix = component.substring(p + 1);
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
