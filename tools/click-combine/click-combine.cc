/*
 * click-combine.cc -- combine several Click configurations at their devices
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include "lexert.hh"
#include "error.hh"
#include "clp.h"
#include "toolutils.hh"
#include "confparse.hh"
#include "straccum.hh"
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
Usage: %s [OPTION]... [ROUTERFILE | ROUTERNAME:FILE | LINKSPEC]\n\
\n\
Options:\n\
  -f, --file FILE       Read router configuration from FILE.\n\
  -o, --output FILE     Write combined configuration to FILE.\n\
  -n, --name NAME       The next router component will be named NAME.\n\
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
static Vector<Hookup> links_from;
static Vector<Hookup> links_to;

static void
read_router(String next_name, int next_number, const char *filename,
	    ErrorHandler *errh)
{
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
}

static int
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
  
  int t1 = r->type_index(class1);
  int t2 = r->type_index(class2);
  int found = -1;
  for (int i = 0; i < r->nelements(); i++) {
    const ElementT &e = r->element(i);
    if (e.type >= 0 && (e.type == t1 || e.type == t2)) {
      Vector<String> words;
      cp_argvec(e.configuration, words);
      if (words.size() >= 1 && words[0] == devname) {
	// found it, but check for duplication
	if (found >= 0) {
	  if (class2)
	    errh->error("more than one `%s(%s)' or `%s(%s)' element in router `%s'", class1.cc(), devname.cc(), class2.cc(), devname.cc(), router_name.cc());
	  else
	    errh->error("more than one `%s(%s)' element in router `%s'",
			class1.cc(), devname.cc(), router_name.cc());
	  found = -2;
	} else if (found == -1)
	  found = i;
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
  int element1 = routers[router1]->eindex(words[2]);
  if (element1 < 0)
    element1 = try_find_device(words[2], "ToDevice", "", router1, errh);
  int element2 = routers[router2]->eindex(words[6]);
  if (element2 < 0)
    element2 = try_find_device(words[6], "FromDevice", "PollDevice", router2, errh);
  if (element1 < 0 || element2 < 0) {
    if (element1 < 0) errh->error("router `%s' has no element or device named `%s'", words[0].cc(), words[2].cc());
    if (element2 < 0) errh->error("router `%s' has no element or device named `%s'", words[4].cc(), words[6].cc());
    return -1;
  }

  // check element types
  String tn1 = routers[router1]->etype_name(element1);
  String tn2 = routers[router2]->etype_name(element2);
  if (tn1 != "ToDevice") {
    errh->warning("router `%s' element `%s' has unexpected class", words[0].cc(), words[2].cc());
    errh->message("  expected ToDevice, got %s", tn1.cc());
  }
  if (tn2 != "FromDevice" && tn2 != "PollDevice") {
    errh->warning("router `%s' element `%s' has unexpected class", words[4].cc(), words[6].cc());
    errh->message("  expected FromDevice or PollDevice, got %s", tn2.cc());
  }
  
  // append link definition
  links_from.push_back(Hookup(router1, element1));
  links_to.push_back(Hookup(router2, element2));
  return -1;
}

static void
make_link(int linki, RouterT *combined, ErrorHandler *)
{
  int router1 = links_from[linki].idx, router2 = links_to[linki].idx;
  int element1 = links_from[linki].port, element2 = links_to[linki].port;
  String name1 = router_names[router1]
    + "/" + routers[router1]->ename(element1);
  String name2 = router_names[router2]
    + "/" + routers[router2]->ename(element2);
  String type1 = routers[router1]->etype_name(element1);
  String type2 = routers[router2]->etype_name(element2);

  // make new configuration string
  String conf;
  {
    Vector<String> words;
    words.push_back(name1);
    words.push_back(type1);
    words.push_back(routers[router1]->econfiguration(element1));
    words.push_back(name2);
    words.push_back(type2);
    words.push_back(routers[router2]->econfiguration(element2));
    conf = cp_unargvec(words);
  }

  // add new element
  int new_type = combined->get_type_index("RouterLink");
  int newe = combined->get_eindex
    ("link" + String(linki + 1), new_type, conf, String());
  int e1 = combined->eindex(name1), e2 = combined->eindex(name2);
  combined->insert_before(newe, Hookup(e1, 0));
  combined->insert_after(newe, Hookup(e2, 0));
  combined->free_element(e1);
  combined->free_element(e2);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-combine: ");

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
      printf("click-combine (Click) %s\n", VERSION);
      printf("Copyright (C) 2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
      read_router(next_name, next_number, clp->arg, errh);
      next_name = String();
      next_number++;
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
      if (const char *s = strchr(clp->arg, ':')) {
	String name(clp->arg, s - clp->arg);
	if (next_name)
	  p_errh->warning("router name specified twice (`%s' and `%s')",
			  next_name.cc(), name.cc());
	read_router(name, next_number, s + 1, errh);
	next_name = String();
	next_number++;
      } else if (const char *s = strchr(clp->arg, '=')) {
	link_texts.push_back(clp->arg);
      } else {
	read_router(next_name, next_number, clp->arg, errh);
	next_name = String();
	next_number++;
      }
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
  for (int i = 0; i < routers.size(); i++) {
    int ei = combined->get_eindex(router_names[i], RouterT::TUNNEL_TYPE,
				  String(), String());
    routers[i]->expand_into(combined, ei, combined, RouterScope(), errh);
  }

  // make links
  if (links_from.size() == 0)
    errh->warning("no links between routers");
  StringAccum links_sa;
  for (int i = 0; i < links_from.size(); i++)
    make_link(i, combined, errh);
  combined->remove_tunnels();
  
  // add elementmap to archive
  {
    if (combined->archive_index("elementmap") < 0)
      combined->add_archive(init_archive_element("elementmap", 0600));
    ArchiveElement &ae = combined->archive("elementmap");
    ElementMap em(ae.data);
    em.add("RouterLink", "", "", "l/h");
    ae.data = em.unparse();
  }

  write_router_file(combined, outf, errh);
  exit(0);
}
