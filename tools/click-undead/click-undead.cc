/*
 * click-undead.cc -- remove dead code from Click
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

static HashMap<String, int> element_ninputs(-1);
static HashMap<String, int> element_noutputs(-1);

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
save_element_nports(RouterT *r)
{
  Vector<int> ninputs(r->nelements(), 0);
  Vector<int> noutputs(r->nelements(), 0);
  
  int nh = r->nhookup();
  const Vector<Hookup> &hfrom = r->hookup_from();
  const Vector<Hookup> &hto = r->hookup_to();
  for (int i = 0; i < nh; i++) {
    const Hookup &hf = hfrom[i], &ht = hto[i];
    if (hf.idx >= 0) {
      if (hf.port >= noutputs[hf.idx])
	noutputs[hf.idx] = hf.port + 1;
      if (ht.port >= ninputs[ht.idx])
	ninputs[ht.idx] = ht.port + 1;
    }
  }

  int nelem = r->nelements();
  for (int i = 0; i < nelem; i++) {
    element_ninputs.insert(r->ename(i), ninputs[i]);
    element_noutputs.insert(r->ename(i), noutputs[i]);
  }
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
skip_over_push(RouterT *r, const Hookup &old_to, const Hookup &new_to)
{
  Vector<int> connv;
  r->find_connections_to(old_to, connv);
  for (int i = 0; i < connv.size(); i++)
    r->change_connection_to(connv[i], new_to);
}

static void
skip_over_pull(RouterT *r, const Hookup &old_from, const Hookup &new_from)
{
  Vector<int> connv;
  r->find_connections_from(old_from, connv);
  for (int i = 0; i < connv.size(); i++)
    r->change_connection_from(connv[i], new_from);
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
    else if (hprev.size() == 1)
      skip_over_pull(r, Hookup(ei, 0), r->hookup_from(hprev[0]));
    else if (hnext.size() == 1)
      skip_over_push(r, Hookup(ei, 0), r->hookup_to(hnext[0]));

    r->kill_element(ei);
  }
}

static bool
remove_redundant_schedulers(RouterT *r, int tindex, bool config_eq_ninputs,
			    ErrorHandler *errh)
{
  if (tindex < 0)
    return false;
  int idle_tindex = r->get_type_index("Idle");

  bool changed = false;
  for (int ei = 0; ei < r->nelements(); ei++) {
    if (r->etype(ei) != tindex)
      continue;
    if (r->noutputs(ei) != 1) {
      errh->lwarning(r->elandmark(ei), "odd connections to `%s'", r->edeclaration(ei).cc());
      continue;
    }
    
    Vector<int> hprev;
    Vector<String> args;
    r->find_connection_vector_to(ei, hprev);
    // check configuration string if we need to
    if (config_eq_ninputs) {
      cp_argvec(r->econfiguration(ei), args);
      if (args.size() != hprev.size())
	continue;
    }
    
    for (int p = 0; p < hprev.size(); p++)
      if (hprev[p] == -1 || (hprev[p] >= 0 && r->etype(r->hookup_from(hprev[p]).idx) == idle_tindex)) {
	// remove that scheduler port
	// check configuration first
	if (config_eq_ninputs) {
	  for (int pp = p + 1; pp < hprev.size(); pp++)
	    args[pp-1] = args[pp];
	  args.pop_back();
	  r->econfiguration(ei) = cp_unargvec(args);
	}

	// now do connections
	int bad_connection = hprev[p];
	for (int pp = p + 1; pp < hprev.size(); pp++) {
	  r->change_connection_to(hprev[pp], Hookup(ei, pp - 1));
	  hprev[pp - 1] = hprev[pp];
	}
	if (bad_connection >= 0)
	  r->kill_connection(bad_connection);
	hprev.pop_back();
	p--;
      }
    
    if (hprev.size() == 1) {
      if (verbose)
	errh->lerror(r->elandmark(ei), "removing redundant scheduler `%s'", r->edeclaration(ei).cc());
      skip_over_pull(r, Hookup(ei, 0), r->hookup_from(hprev[0]));
      r->kill_element(ei);
      changed = true;
    }

    // save number of inputs so we don't attach new Idles
    element_ninputs.insert(r->ename(ei), hprev.size());
  }

  return changed;
}

static bool
remove_redundant_tee_ports(RouterT *r, int tindex, bool is_pull_tee,
			   ErrorHandler *errh)
{
  if (tindex < 0)
    return false;
  int idle_tindex = r->get_type_index("Idle");

  bool changed = false;
  for (int ei = 0; ei < r->nelements(); ei++) {
    if (r->etype(ei) != tindex)
      continue;
    if (r->ninputs(ei) != 1) {
      errh->lwarning(r->elandmark(ei), "odd connections to `%s'", r->edeclaration(ei).cc());
      continue;
    }
    
    Vector<int> hnext;
    Vector<String> args;
    r->find_connection_vector_from(ei, hnext);
    r->econfiguration(ei) = "";
    
    for (int p = (is_pull_tee ? 1 : 0); p < hnext.size(); p++)
      if (hnext[p] == -1 || (hnext[p] >= 0 && r->etype(r->hookup_from(hnext[p]).idx) == idle_tindex)) {
	// remove that tee port
	int bad_connection = hnext[p];
	for (int pp = p + 1; pp < hnext.size(); pp++) {
	  r->change_connection_from(hnext[pp], Hookup(ei, pp - 1));
	  hnext[pp - 1] = hnext[pp];
	}
	if (bad_connection >= 0)
	  r->kill_connection(bad_connection);
	hnext.pop_back();
	p--;
      }
    
    if (hnext.size() == 1) {
      if (verbose)
	errh->lerror(r->elandmark(ei), "removing redundant tee `%s'", r->edeclaration(ei).cc());
      if (is_pull_tee) {
	Vector<int> hprev;
	r->find_connection_vector_to(ei, hprev);
	skip_over_pull(r, Hookup(ei, 0), r->hookup_from(hprev[0]));
      } else
	skip_over_push(r, Hookup(ei, 0), r->hookup_to(hnext[0]));
      r->kill_element(ei);
      changed = true;
    }

    // save number of inputs so we don't attach new Idles
    element_noutputs.insert(r->ename(ei), hnext.size());
  }

  return changed;
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
  Bitvector dead(r->nelements(), false);

  // find initial sources and sinks
  for (int ei = 0; ei < r->nelements(); ei++) {
    const ElementT &e = r->element(ei);
    if (e.live()) {
      int nin = processing.ninputs(ei);
      int nout = processing.noutputs(ei);
      int source_flag = em->flag_value(r->type_name(e.type), 'S');

      if (source_flag == 0) {	// neither source nor sink
	dead[ei] = true;
	continue;
      } else if (source_flag == 1) { // source
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
      int f = hf[i].idx, t = ht[i].idx;
      if (f >= 0 && !sources[t] && sources[f] && !dead[t])
	sources[t] = changed = true;
    }
  }
  
  // spread sinks
  changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < nh; i++) {
      int f = hf[i].idx, t = ht[i].idx;
      if (f >= 0 && !sinks[f] && sinks[t] && !dead[f])
	sinks[f] = changed = true;
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
replace_blank_ports(RouterT *r)
{
  int ne = r->nelements();
  int idle_index = -1;
  int idle_next_in = 0, idle_next_out = 0;
  for (int ei = 0; ei < ne; ei++) {
    if (r->edead(ei))
      continue;
    Vector<int> connv;

    r->find_connection_vector_to(ei, connv);
    connv.resize(element_ninputs[r->ename(ei)], -1);
    for (int p = 0; p < connv.size(); p++)
      if (connv[p] == -1) {	// unconnected port
	if (idle_index < 0)
	  idle_index = r->get_anon_eindex(r->get_type_index("Idle"), "", "<click-undead>");
	r->add_connection(idle_index, idle_next_out++, p, ei);
      }

    r->find_connection_vector_from(ei, connv);
    connv.resize(element_noutputs[r->ename(ei)], -1);
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

  // save numbers of inputs and outputs for later
  save_element_nports(r);

  // remove elements who make static routing decisions
  remove_static_switches(r, default_errh);
  remove_static_pull_switches(r, default_errh);

  // remove dead elements to improve processing checking
  r->remove_dead_elements();
  
  // find live elements in the drivers
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

  // remove Idles
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
  
  // remove redundant schedulers
  while (1) {
    int nchanges = 0;
    nchanges += remove_redundant_schedulers(r, r->type_index("RoundRobinSched"), false, default_errh);
    nchanges += remove_redundant_schedulers(r, r->type_index("PrioSched"), false, default_errh);
    nchanges += remove_redundant_schedulers(r, r->type_index("StrideSched"), true, default_errh);
    nchanges += remove_redundant_tee_ports(r, r->type_index("Tee"), false, default_errh);
    nchanges += remove_redundant_tee_ports(r, r->type_index("PullTee"), true, default_errh);
    remove_nulls(r, r->type_index("Null"), default_errh);
    if (!nchanges) break;
  }

  // hook up blanked-out live ports to a new Idle
  replace_blank_ports(r);
  
  // NOW remove bad elements
  r->remove_dead_elements();
  
  if (config_only) {
    String config = r->configuration_string();
    fwrite(config.data(), 1, config.length(), outf);
  } else
    write_router_file(r, outf, default_errh);
  
  exit(0);
}
