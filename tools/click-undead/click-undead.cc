/*
 * click-undead.cc -- specialize Click classifiers
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "processingt.hh"
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "clp.h"
#include "toolutils.hh"
#include "bitvector.hh"
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
#define KERNEL_OPT		305
#define USERLEVEL_OPT		306
#define CONFIG_OPT		307
#define VERBOSE_OPT		310

static Clp_Option options[] = {
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String::Initializer string_initializer;
static bool verbose;

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
`Click-undead' transforms a router configuration by removing dead code. This\n\
includes any elements that are not connected both to a packet source and to a\n\
packet sink. It also removes redundant elements, such as Null and StaticSwitch.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -o, --output FILE             Write output to FILE.\n\
  -k, --kernel                  Configuration is for Linux kernel driver.\n\
  -u, --user                    Configuration is for user-level driver.\n\
  -c, --config                  Write new configuration only.\n\
  -V, --verbose                 Print debugging information.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


static void
remove_static_switches(RouterT *r, ErrorHandler *errh)
{
  int tindex = r->type_index("StaticSwitch");
  if (tindex < 0)
    return;
  int idle_tindex = r->get_type_index("Idle");

  for (int ei = 0; ei < r->nelements(); ei++) {
    if (r->etype(ei) != tindex)
      continue;
    
    String config = cp_uncomment(r->econfiguration(ei));
    int val;
    if (!cp_integer(config, &val)) {
      errh->lerror(r->elandmark(ei), "%s: bad configuration `StaticSwitch(%s)'", r->ename(ei).cc(), config.cc());
      val = -1;
    }

    int noutputs = r->noutputs(ei);
    Vector<int> connv_out;
    r->find_connection_vector_from(ei, connv_out);
    for (int i = 0; i < connv_out.size(); i++)
      if (connv_out[i] < 0) {
	errh->lerror(r->elandmark(ei), "odd connections from `%s'", r->edeclaration(ei).cc());
	break;
      }
    
    int idle = r->get_anon_eindex(idle_tindex, "", "<click-undead>");
    int idle_in = 0, idle_out = 0;
    
    Hookup jump_hook;
    if (val < 0 || val >= noutputs || connv_out[val] < 0)
      jump_hook = Hookup(idle, idle_in++);
    else
      jump_hook = r->hookup_to()[connv_out[val]];
    
    Vector<Hookup> conns_to;
    r->find_connections_to(Hookup(ei, 0), conns_to);
    for (int j = 0; j < conns_to.size(); j++) {
      int k = r->find_connection(conns_to[j], Hookup(ei, 0));
      r->change_connection_to(k, jump_hook);
    }

    for (int j = 0; j < connv_out.size(); j++)
      if (j != val)
	r->change_connection_from(connv_out[j], Hookup(idle, idle_out++));

    r->kill_element(ei);
  }
}

static void
remove_static_pull_switches(RouterT *r, ErrorHandler *errh)
{
  int tindex = r->type_index("StaticPullSwitch");
  if (tindex < 0)
    return;
  int idle_tindex = r->get_type_index("Idle");

  for (int ei = 0; ei < r->nelements(); ei++) {
    if (r->etype(ei) != tindex)
      continue;
    
    String config = cp_uncomment(r->econfiguration(ei));
    int val;
    if (!cp_integer(config, &val)) {
      errh->lerror(r->elandmark(ei), "%s: bad configuration `StaticSwitch(%s)'", r->ename(ei).cc(), config.cc());
      val = -1;
    }

    int ninputs = r->ninputs(ei);
    Vector<int> connv_in;
    r->find_connection_vector_to(ei, connv_in);
    for (int i = 0; i < connv_in.size(); i++)
      if (connv_in[i] < 0) {
	errh->lerror(r->elandmark(ei), "odd connections to `%s'", r->edeclaration(ei).cc());
	break;
      }
    
    int idle = r->get_anon_eindex(idle_tindex, "", "<click-undead>");
    int idle_in = 0, idle_out = 0;
    
    Hookup jump_hook;
    if (val < 0 || val >= ninputs || connv_in[val] < 0)
      jump_hook = Hookup(idle, idle_out++);
    else
      jump_hook = r->hookup_from()[connv_in[val]];
    
    Vector<Hookup> conns_from;
    r->find_connections_from(Hookup(ei, 0), conns_from);
    for (int j = 0; j < conns_from.size(); j++) {
      int k = r->find_connection(Hookup(ei, 0), conns_from[j]);
      r->change_connection_from(k, jump_hook);
    }

    for (int j = 0; j < connv_in.size(); j++)
      if (j != val)
	r->change_connection_to(connv_in[j], Hookup(idle, idle_in++));

    r->kill_element(ei);
  }
}

static void
remove_nulls(RouterT *r, int tindex, ErrorHandler *errh)
{
  if (tindex < 0)
    return;

  for (int ei = 0; ei < r->nelements(); ei++) {
    if (r->etype(ei) != tindex)
      continue;
    int nin = r->ninputs(ei), nout = r->noutputs(ei);
    if (nin != 1 || nout != 1) {
      errh->lwarning(r->elandmark(ei), "odd connections to `%s'", r->edeclaration(ei).cc());
      continue;
    }
    
    Vector<int> hprev, hnext;
    r->find_connections_to(Hookup(ei, 0), hprev);
    r->find_connections_from(Hookup(ei, 0), hnext);
    if (hprev.size() > 1 && hnext.size() > 1)
      errh->lwarning(r->elandmark(ei), "odd connections to `%s'", r->edeclaration(ei).cc());
    else if (hprev.size() == 1) {
      Hookup prev_port = r->hookup_from(hprev[0]);
      for (int j = 0; j < hnext.size(); j++)
	r->change_connection_from(hnext[j], prev_port);
    } else if (hnext.size() == 1) {
      Hookup next_port = r->hookup_to(hnext[0]);
      for (int j = 0; j < hprev.size(); j++)
	r->change_connection_to(hprev[j], next_port);
    }

    r->kill_element(ei);
  }
}

static void
find_live_elements(const RouterT *r, const char *filename,
		   const Vector<int> &elementmap_indexes,
		   const ElementMap &full_elementmap, int driver,
		   bool indifferent, ProcessingT &processing,
		   Bitvector &live_elements, ErrorHandler *errh)
{
  if (!indifferent && !full_elementmap.driver_compatible(elementmap_indexes, driver)) {
    errh->error("%s: configuration incompatible with %s driver", filename, ElementMap::driver_name(driver));
    return;
  }

  const ElementMap *em = &full_elementmap;
  if (!indifferent) {
    ElementMap *new_em = new ElementMap(full_elementmap);
    new_em->limit_driver(driver);
    em = new_em;
  }

  // get processing
  processing.reset(r, *em, errh);
  // ... it will report errors as required

  Bitvector sources(r->nelements(), false);
  Bitvector sinks(r->nelements(), false);

  // find initial sources and sinks
  for (int ei = 0; ei < r->nelements(); ei++) {
    const ElementT &e = r->element(ei);
    if (e.live()) {
      int source_flag = em->flag_value(r->type_name(e.type), 'S');

      if (source_flag == 0)	// neither source nor sink
	continue;
      else if (source_flag == 1) { // source
	sources[ei] = true;
	if (verbose)
	  errh->lmessage(r->elandmark(ei), "`%s' is source", r->edeclaration(ei).cc());
	continue;
      } else if (source_flag == 2) { // sink
	sinks[ei] = true;
	if (verbose)
	  errh->lmessage(r->elandmark(ei), "`%s' is sink", r->edeclaration(ei).cc());
	continue;
      } else if (source_flag == 3) { // source and sink
	sources[ei] = sinks[ei] = true;
	if (verbose)
	  errh->lmessage(r->elandmark(ei), "`%s' is source and sink", r->edeclaration(ei).cc());
	continue;
      } else if (source_flag > 0)
	errh->lwarning(r->elandmark(ei), "`%s' has strange source/sink flag value %d", r->edeclaration(ei).cc(), source_flag);

      // if no source/sink flags, make an educated guess
      int nin = processing.ninputs(ei), nout = processing.noutputs(ei);
      if (nin == 0) {
	for (int p = 0; p < nout; p++)
	  if (processing.output_is_push(ei, p)) {
	    sources[ei] = true;
	    if (verbose)
	      errh->lmessage(r->elandmark(ei), "assuming `%s' is source", r->edeclaration(ei).cc());
	    break;
	  }
      }
      if (nout == 0) {
	for (int p = 0; p < nin; p++)
	  if (processing.input_is_pull(ei, p)) {
	    sinks[ei] = true;
	    if (verbose)
	      errh->lmessage(r->elandmark(ei), "assuming `%s' is sink", r->edeclaration(ei).cc());
	    break;
	  }
      }
    }
  }

  int nh = r->nhookup();
  const Vector<Hookup> &hf = r->hookup_from();
  const Vector<Hookup> &ht = r->hookup_to();
  
  // spread sources
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < nh; i++) {
      if (hf[i].idx >= 0 && !sources[ht[i].idx] && sources[hf[i].idx])
	sources[ht[i].idx] = changed = true;
    }
  }
  
  // spread sinks
  changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < nh; i++) {
      if (hf[i].idx >= 0 && !sinks[hf[i].idx] && sinks[ht[i].idx])
	sinks[hf[i].idx] = changed = true;
    }
  }

  // live = (sources & sinks) | independently_live
  live_elements = sources & sinks;

  // find independently live elements
  for (int ei = 0; ei < r->nelements(); ei++) {
    const ElementT &e = r->element(ei);
    if (e.live() && !live_elements[ei]) {
      int live_flag = em->flag_value(r->type_name(e.type), 'L');
      if (live_flag == 0)	// not live
	continue;
      else if (live_flag == 1) { // live
	live_elements[ei] = true;
	continue;
      } else if (live_flag > 0)
	errh->lwarning(r->elandmark(ei), "`%s' has strange live flag value %d", r->edeclaration(ei).cc(), live_flag);

      // if no live flag, make an educated guess
      if (processing.ninputs(ei) == 0 && processing.noutputs(ei) == 0) {
	live_elements[ei] = true;
	if (verbose)
	  errh->lmessage(r->elandmark(ei), "assuming `%s' is live", r->edeclaration(ei).cc());
      }
    }
  }

  if (!indifferent)
    delete em;
}

static void
replace_blank_ports(RouterT *r, ProcessingT &processing)
{
  int ne = r->nelements();
  int idle_index = -1;
  int idle_next_in = 0, idle_next_out = 0;
  for (int ei = 0; ei < ne; ei++) {
    if (r->edead(ei))
      continue;
    Vector<int> connv;
    
    r->find_connection_vector_to(ei, connv);
    connv.resize(processing.ninputs(ei), -1);
    for (int p = 0; p < connv.size(); p++)
      if (connv[p] == -1) {	// unconnected port
	if (idle_index < 0)
	  idle_index = r->get_anon_eindex(r->get_type_index("Idle"), "", "<click-undead>");
	r->add_connection(idle_index, idle_next_out++, p, ei);
      }

    r->find_connection_vector_from(ei, connv);
    connv.resize(processing.noutputs(ei), -1);
    for (int p = 0; p < connv.size(); p++)
      if (connv[p] == -1) {	// unconnected port
	if (idle_index < 0)
	  idle_index = r->get_anon_eindex(r->get_type_index("Idle"), "", "<click-undead>");
	r->add_connection(ei, p, idle_next_in++, idle_index);
      }
  }
}


int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler *default_errh = new FileErrorHandler(stderr);
  ErrorHandler::static_initialize(default_errh);
  ErrorHandler *p_errh = new PrefixErrorHandler(default_errh, "click-undead: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool config_only = false;
  int check_kernel = -1;
  int check_userlevel = -1;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-undead (Click) %s\n", VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case ROUTER_OPT:
     case Clp_NotOption:
      if (router_file) {
	p_errh->error("router file specified twice");
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

     case CONFIG_OPT:
      config_only = !clp->negated;
      break;

     case KERNEL_OPT:
      check_kernel = (clp->negated ? 0 : 1);
      break;
      
     case USERLEVEL_OPT:
      check_userlevel = (clp->negated ? 0 : 1);
      break;

     case VERBOSE_OPT:
      verbose = !clp->negated;
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
  RouterT *r = read_router_file(router_file, default_errh);
  if (!r || default_errh->nerrors() > 0)
    exit(1);
  r->flatten(default_errh);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      default_errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find and parse `elementmap'
  ElementMap elementmap;
  {
    String fn = clickpath_find_file("elementmap", "share", CLICK_SHAREDIR);
    if (!fn)
      p_errh->fatal("cannot find `elementmap' in CLICKPATH or `%s'", CLICK_SHAREDIR);
    else
      elementmap.parse(file_string(fn, default_errh));
  }

  // parse `elementmap' from router archive
  if (r->archive_index("elementmap") >= 0)
    elementmap.parse(r->archive("elementmap").data);

  // check configuration for driver indifference
  Vector<int> elementmap_indexes;
  elementmap.map_indexes(r, elementmap_indexes, default_errh);
  bool indifferent = elementmap.driver_indifferent(elementmap_indexes);
  if (indifferent) {
    if (check_kernel < 0 && check_userlevel < 0)
      // only bother to check one of them
      check_kernel = 0;
  } else {
    if (check_kernel < 0 && check_userlevel < 0) {
      check_kernel = elementmap.driver_compatible(elementmap_indexes, ElementMap::DRIVER_LINUXMODULE);
      check_userlevel = elementmap.driver_compatible(elementmap_indexes, ElementMap::DRIVER_USERLEVEL);
    }
  }

  // remove meaningless elements
  remove_static_switches(r, default_errh);
  remove_static_pull_switches(r, default_errh);
  remove_nulls(r, r->type_index("Null"), default_errh);

  // remove dead elements to improve processing checking
  r->remove_dead_elements();
  
  // actually check the drivers
  Bitvector kernel_vec, user_vec;
  ProcessingT processing;
  if (check_kernel > 0)
    find_live_elements(r, router_file, elementmap_indexes, elementmap,
		       ElementMap::DRIVER_LINUXMODULE, indifferent,
		       processing, kernel_vec, default_errh);
  if (check_userlevel > 0)
    find_live_elements(r, router_file, elementmap_indexes, elementmap,
		       ElementMap::DRIVER_USERLEVEL, indifferent,
		       processing, user_vec, default_errh);

  // an element is live if it's live in either driver
  Bitvector live_vec = kernel_vec | user_vec;
  if (!live_vec.size() && r->nelements())
    exit(1);
  
  for (int i = 0; i < r->nelements(); i++)
    if (!live_vec[i]) {
      if (verbose)
	default_errh->lmessage(r->elandmark(i), "removing `%s'", r->edeclaration(i).cc());
      r->kill_element(i);
    }
  
  // remove dead connections (not elements yet: keep indexes in 'processing'
  // the same)
  r->kill_bad_connections();
  r->check();
    
  // hook up blanked-out live ports to a new Idle
  replace_blank_ports(r, processing);

  // NOW remove bad elements
  r->remove_dead_elements();
  
  if (config_only) {
    String config = r->configuration_string();
    fwrite(config.data(), 1, config.length(), outf);
  } else
    write_router_file(r, outf, default_errh);
  
  exit(0);
}
