/*
 * click-combine.cc -- combine several Click configurations at their devices
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
#include <click/variableenv.hh>
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
#define LINK_OPT		305

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "link", 'l', LINK_OPT, Clp_ArgString, 0 },
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
`Click-combine' combines several Click router configurations at their network\n\
devices and writes the combined configuration to the standard output. The\n\
combination is controlled by link specifications. The click-uncombine tool can\n\
extract components from these combined configurations.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE | ROUTERNAME=FILE | LINKSPEC]\n\
\n\
Options:\n\
  -o, --output FILE     Write combined configuration to FILE.\n\
  -n, --name NAME       The next router component name is NAME.\n\
  -f, --file FILE       Read router component configuration from FILE.\n\
  -l, --link LINKSPEC   Add a link between router components. LINKSPEC has the\n\
                        form `NAME1.COMP1=NAME2.COMP2'. Each NAME is a router\n\
                        component name. Each COMP is either an element name or\n\
                        a device name (for linking at From/To/PollDevices).\n\
      --help            Print this message and exit.\n\
  -v, --version         Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Vector<String> router_names;
static Vector<RouterT *> routers;
typedef PortT RouterPortT;	// except that `port' is the router index
static Vector<RouterPortT> links_from;
static Vector<RouterPortT> links_to;
static Vector<int> link_id;

static void
read_router(String name, String &next_name, int &next_number,
	    const char *filename, ErrorHandler *errh)
{
  if (name && next_name)
    errh->warning("router name specified twice (`%s' and `%s')",
		  next_name.cc(), name.cc());
  else if (name)
    next_name = name;
  
  RouterT *r = read_router_file(filename, errh);
  
  if (r) {
    r->flatten(errh);
    if (next_name) {
      int span = strspn(next_name.cc(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_/@0123456789");
      if (span != next_name.length() || strstr(next_name.cc(), "//") != 0
	  || next_name[0] == '/')
	errh->error("router name `%s' is not a legal Click identifier", next_name.cc());
      router_names.push_back(next_name);
    } else
      router_names.push_back(String(next_number));
    routers.push_back(r);
  }

  next_name = String();
  next_number++;
}

static ElementT *
try_find_device(String devname, String class1, String class2,
		int rn, ErrorHandler *errh)
{
  RouterT *r = routers[rn];
  String router_name = router_names[rn];
  
  // fix config
  {
    Vector<String> words;
    cp_argvec(devname, words);
    devname = words[0];
  }
  
  ElementClassT *t1 = r->try_type(class1);
  ElementClassT *t2 = r->try_type(class2);
  ElementT *found = 0;
  bool duplicate = false;
  for (int i = 0; i < r->nelements(); i++) {
    ElementT *e = r->element(i);
    if (e->live() && (e->type() == t1 || e->type() == t2)) {
      Vector<String> words;
      cp_argvec(e->configuration(), words);
      if (words.size() >= 1 && words[0] == devname) {
	// found it, but check for duplication
	if (!found && !duplicate)
	  found = e;
	else if (!duplicate) {
	  if (class2)
	    errh->error("more than one `%s(%s)' or `%s(%s)' element in router `%s'", class1.cc(), devname.cc(), class2.cc(), devname.cc(), router_name.cc());
	  else
	    errh->error("more than one `%s(%s)' element in router `%s'",
			class1.cc(), devname.cc(), router_name.cc());
	  duplicate = true;
	  found = 0;
	}
      }
    }
  }
  
  return found;
}

static int
parse_link(String text, ErrorHandler *errh)
{
  // separate into words
  Vector<String> words;
  {
    StringAccum sa;
    const char *s = text.data();
    for (int i = 0; i < text.length(); i++)
      if (s[i] == '.' || s[i] == '=')
	sa << ' ' << s[i] << ' ';
      else
	sa << s[i];
    cp_spacevec(sa.take_string(), words);
  }

  // check for errors
  if (words.size() != 7 || words[1] != "." || words[3] != "="
      || words[5] != ".")
    return errh->error("bad link definition `%s'", text.cc());

  // find pieces
  int router1 = -1, router2 = -1;
  for (int i = 0; i < routers.size(); i++) {
    if (router_names[i] == words[0])
      router1 = i;
    if (router_names[i] == words[4])
      router2 = i;
  }
  if (router1 < 0 || router2 < 0) {
    if (router1 < 0) errh->error("no router named `%s'", words[0].cc());
    if (router2 < 0) errh->error("no router named `%s'", words[4].cc());
    return -1;
  }
  ElementT *element1 = routers[router1]->element(words[2]);
  if (!element1)
    element1 = try_find_device(words[2], "ToDevice", "", router1, errh);
  ElementT *element2 = routers[router2]->element(words[6]);
  if (!element2)
    element2 = try_find_device(words[6], "FromDevice", "PollDevice", router2, errh);
  if (!element1 || !element2) {
    if (!element1)
      errh->error("router `%s' has no element or device named `%s'", words[0].cc(), words[2].cc());
    if (!element2)
      errh->error("router `%s' has no element or device named `%s'", words[4].cc(), words[6].cc());
    return -1;
  }

  // check element types
  String tn1 = element1->type_name();
  String tn2 = element2->type_name();
  if (tn1 != "ToDevice") {
    errh->warning("router `%s' element `%s' has unexpected class", words[0].cc(), words[2].cc());
    errh->message("  expected ToDevice, got %s", tn1.cc());
  }
  if (tn2 != "FromDevice" && tn2 != "PollDevice") {
    errh->warning("router `%s' element `%s' has unexpected class", words[4].cc(), words[6].cc());
    errh->message("  expected FromDevice or PollDevice, got %s", tn2.cc());
  }
  
  // append link definition
  links_from.push_back(RouterPortT(element1, router1));
  links_to.push_back(RouterPortT(element2, router2));
  return -1;
}

static void
frob_nested_routerlink(ElementT *e)
{
  String prefix = e->name().substring(0, e->name().find_left('/') + 1);
  assert(prefix.length() > 1 && prefix.back() == '/');
  Vector<String> words;
  cp_argvec(e->configuration(), words);
  for (int i = 0; i < words.size(); i += 2)
    words[i] = prefix + words[i];
  e->configuration() = cp_unargvec(words);
}

static int
combine_links(ErrorHandler *errh)
{
  // check for same name used as both source and destination
  int before = errh->nerrors();
  for (int i = 1; i < links_from.size(); i++)
    for (int j = 0; j < i; j++)
      if (links_from[i] == links_to[j] || links_from[j] == links_to[i]) {
	const RouterPortT &h = links_from[i];
	errh->error("router `%s' element `%s' used as both source and destination", router_names[h.port].cc(), h.elt->name_cc());
      }
  if (errh->nerrors() != before)
    return -1;
  
  // combine links
  link_id.clear();
  for (int i = 0; i < links_from.size(); i++)
    link_id.push_back(i);
  bool done = false;
  while (!done) {
    done = true;
    for (int i = 1; i < links_from.size(); i++)
      for (int j = 0; j < i; j++)
	if ((links_from[i] == links_from[j] || links_to[i] == links_to[j])
	    && link_id[i] != link_id[j]) {
	  link_id[i] = link_id[j];
	  done = false;
	}
  }

  return 0;
}

static void
make_link(const Vector<RouterPortT> &from, const Vector<RouterPortT> &to,
	  RouterT *combined)
{
  static int linkno = 0;
  
  Vector<RouterPortT> all(from);
  for (int i = 0; i < to.size(); i++)
    all.push_back(to[i]);

  Vector<ElementT *> combes;
  Vector<String> words;
  for (int i = 0; i < all.size(); i++) {
    int r = all[i].port;
    ElementT *e = all[i].elt;
    String name = router_names[r] + "/" + e->name();
    combes.push_back(combined->element(name));
    assert(combes.back());
    words.push_back(router_names[r] + " " + e->name() + " " + e->type_name());
    words.push_back(e->configuration());
  }

  // add new element
  ElementClassT *link_type = combined->get_type("RouterLink");
  ElementT *newe = combined->get_element
    ("link" + String(++linkno), link_type, cp_unargvec(words), "<click-combine>");

  for (int i = 0; i < from.size(); i++) {
    combined->insert_before(PortT(newe, i), PortT(combes[i], 0));
    combined->free_element(combes[i]);
  }
  for (int j = from.size(); j < combes.size(); j++) {
    combined->insert_after(PortT(newe, j - from.size()), PortT(combes[j], 0));
    combined->free_element(combes[j]);
  }
}

static void
add_links(RouterT *r)
{
  Vector<int> done(link_id.size(), 0);
  for (int i = 0; i < links_from.size(); i++)
    if (!done[link_id[i]]) {
      // find all input & output ports
      Vector<RouterPortT> from, to;
      for (int j = 0; j < links_from.size(); j++)
	if (link_id[j] == link_id[i]) {
	  if (links_from[j].index_in(from) < 0)
	    from.push_back(links_from[j]);
	  if (links_to[j].index_in(to) < 0)
	    to.push_back(links_to[j]);
	}
      // add single link
      make_link(from, to, r);
      done[link_id[i]] = 1;
    }
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-combine: ");
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *output_file = 0;
  String next_name;
  int next_number = 1;
  Vector<String> link_texts;

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-combine (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
      read_router(String(), next_name, next_number, clp->arg, errh);
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case NAME_OPT:
      if (next_name)
	p_errh->warning("router name specified twice");
      next_name = clp->arg;
      break;

     case LINK_OPT:
      link_texts.push_back(clp->arg);
      break;
      
     case Clp_NotOption:
      if (const char *s = strchr(clp->arg, ':'))
	read_router(String(clp->arg, s - clp->arg), next_name, next_number, s + 1, errh);
      else if (const char *eq = strchr(clp->arg, '=')) {
	const char *dot = strchr(clp->arg, '.');
	if (!dot || dot > eq)
	  read_router(String(clp->arg, eq - clp->arg), next_name, next_number, eq + 1, errh);
	else
	  link_texts.push_back(clp->arg);
      } else
	read_router(String(), next_name, next_number, clp->arg, errh);
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
  // no routers is an error
  if (routers.size() == 0)
    p_errh->fatal("no routers specified");
  
  // check that routers are named differently
  HashMap<String, int> name_map(-1);
  for (int i = 0; i < routers.size(); i++) {
    if (name_map[router_names[i]] >= 0)
      p_errh->fatal("two routers named `%s'", router_names[i].cc());
    name_map.insert(router_names[i], i);
  }

  // define links
  for (int i = 0; i < link_texts.size(); i++)
    parse_link(link_texts[i], p_errh);

  // exit if there have been errors
  if (errh->nerrors() != 0)
    exit(1);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // combine routers
  RouterT *combined = new RouterT;
  for (int i = 0; i < routers.size(); i++)
    routers[i]->expand_into(combined, VariableEnvironment(router_names[i]), errh);

  // exit if there have been errors (again)
  if (errh->nerrors() != 0)
    exit(1);

  // nested combinations: change config strings of included RouterLinks
  ElementClassT *link_type = combined->try_type("RouterLink");
  for (RouterT::type_iterator x = combined->first_element(link_type); x; x++)
    frob_nested_routerlink(x);

  // make links
  if (links_from.size() == 0)
    errh->warning("no links between routers");
  if (combine_links(p_errh) < 0)
    exit(1);
  add_links(combined);
  combined->remove_tunnels();
  
  // add elementmap to archive
  {
    ElementMap em;
    if (link_id.size())
	em.add("RouterLink", "", "", "l/h", "x/x", "S3", "", "");
    // add data from included elementmaps
    for (int i = 0; i < routers.size(); i++)
      if (routers[i]->archive_index("elementmap") >= 0) {
	ArchiveElement &nae = routers[i]->archive("elementmap");
	em.parse(nae.data);
      }
    if (!em.empty()) {
	combined->add_archive(init_archive_element("elementmap", 0600));
	ArchiveElement &ae = combined->archive("elementmap");
	ae.data = em.unparse();
    }
  }

  // add componentmap to archive
  {
    combined->add_archive(init_archive_element("componentmap", 0600));
    ArchiveElement &ae = combined->archive("componentmap");
    StringAccum sa;
    for (int i = 0; i < routers.size(); i++) {
      sa << router_names[i] << '\n';
      if (routers[i]->archive_index("componentmap") >= 0) {
	ArchiveElement &nae = routers[i]->archive("componentmap");
	Vector<String> combines;
	cp_spacevec(nae.data, combines);
	for (int j = 0; j < combines.size(); j++)
	  sa << router_names[i] << '/' << combines[j] << '\n';
      }
    }
    ae.data = sa.take_string();
  }
  
  write_router_file(combined, outf, errh);
  exit(0);
}
