/*
 * click-undead.cc -- remove dead code from Click
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include "routert.hh"
#include "lexert.hh"
#include "processingt.hh"
#include "elementmap.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/driver.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include <click/bitvector.hh>
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
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define OUTPUT_OPT		304
#define KERNEL_OPT		305
#define USERLEVEL_OPT		306
#define CONFIG_OPT		307
#define VERBOSE_OPT		310

static Clp_Option options[] = {
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
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
  -C, --clickpath PATH          Use PATH for CLICKPATH.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


static void
save_element_nports(RouterT *r)
{
  for (RouterT::iterator x = r->first_element(); x; x++) {
    element_ninputs.insert(x->name(), x->ninputs());
    element_noutputs.insert(x->name(), x->noutputs());
  }
}

static void
remove_static_switches(RouterT *r, ErrorHandler *errh)
{
  ElementClassT *t = r->try_type("StaticSwitch");
  if (!t)
    return;
  ElementClassT *idlet = r->get_type("Idle");

  for (RouterT::type_iterator x = r->first_element(t); x; x++) {
    assert(x->type() == t);
    
    String config = cp_uncomment(x->configuration());
    int val;
    if (!cp_integer(config, &val)) {
      errh->lerror(x->landmark(), "%s: bad configuration `StaticSwitch(%s)'", x->name_cc(), config.cc());
      val = -1;
    }

    Vector<int> connv_out;
    r->find_connection_vector_from(x, connv_out);
    for (int i = 0; i < connv_out.size(); i++)
      if (connv_out[i] < 0) {
	errh->lerror(x->landmark(), "odd connections from `%s'", x->declaration().cc());
	break;
      }
    
    ElementT *idle = r->add_anon_element(idlet, "", "<click-undead>");
    int idle_in = 0, idle_out = 0;
    
    PortT jump_hook;
    if (val < 0 || val >= x->noutputs() || connv_out[val] < 0)
      jump_hook = PortT(idle, idle_in++);
    else
      jump_hook = r->connection(connv_out[val]).to();
    
    Vector<PortT> conns_to;
    r->find_connections_to(PortT(x, 0), conns_to);
    for (int j = 0; j < conns_to.size(); j++) {
      int k = r->find_connection(conns_to[j], PortT(x, 0));
      r->change_connection_to(k, jump_hook);
    }

    for (int j = 0; j < connv_out.size(); j++)
      if (j != val)
	r->change_connection_from(connv_out[j], PortT(idle, idle_out++));

    x->kill();
  }
}

static void
remove_static_pull_switches(RouterT *r, ErrorHandler *errh)
{
  ElementClassT *t = r->try_type("StaticPullSwitch");
  if (!t)
    return;
  ElementClassT *idlet = r->get_type("Idle");

  for (RouterT::type_iterator x = r->first_element(t); x; x++) {
    assert(x->type() == t);
    
    String config = cp_uncomment(x->configuration());
    int val;
    if (!cp_integer(config, &val)) {
      errh->lerror(x->landmark(), "%s: bad configuration `StaticSwitch(%s)'", x->name_cc(), config.cc());
      val = -1;
    }

    Vector<int> connv_in;
    r->find_connection_vector_to(x, connv_in);
    for (int i = 0; i < connv_in.size(); i++)
      if (connv_in[i] < 0) {
	errh->lerror(x->landmark(), "odd connections to `%s'", x->declaration().cc());
	break;
      }
    
    ElementT *idle = r->add_anon_element(idlet, "", "<click-undead>");
    int idle_in = 0, idle_out = 0;
    
    PortT jump_hook;
    if (val < 0 || val >= x->ninputs() || connv_in[val] < 0)
      jump_hook = PortT(idle, idle_out++);
    else
      jump_hook = r->connection(connv_in[val]).from();
    
    Vector<PortT> conns_from;
    r->find_connections_from(PortT(x, 0), conns_from);
    for (int j = 0; j < conns_from.size(); j++) {
      int k = r->find_connection(PortT(x, 0), conns_from[j]);
      r->change_connection_from(k, jump_hook);
    }

    for (int j = 0; j < connv_in.size(); j++)
      if (j != val)
	r->change_connection_to(connv_in[j], PortT(idle, idle_in++));

    x->kill();
  }
}

static void
skip_over_push(RouterT *r, const PortT &old_to, const PortT &new_to)
{
  Vector<int> connv;
  r->find_connections_to(old_to, connv);
  for (int i = 0; i < connv.size(); i++)
    r->change_connection_to(connv[i], new_to);
}

static void
skip_over_pull(RouterT *r, const PortT &old_from, const PortT &new_from)
{
  Vector<int> connv;
  r->find_connections_from(old_from, connv);
  for (int i = 0; i < connv.size(); i++)
    r->change_connection_from(connv[i], new_from);
}

static void
remove_nulls(RouterT *r, ElementClassT *t, ErrorHandler *errh)
{
  if (!t)
    return;

  for (RouterT::type_iterator x = r->first_element(t); x; x++) {
    assert(x->type() == t);
    if (x->ninputs() != 1 || x->noutputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to `%s'", x->declaration().cc());
      continue;
    }
    
    Vector<int> hprev, hnext;
    r->find_connections_to(PortT(x, 0), hprev);
    r->find_connections_from(PortT(x, 0), hnext);
    if (hprev.size() > 1 && hnext.size() > 1)
      errh->lwarning(x->landmark(), "odd connections to `%s'", x->declaration().cc());
    else if (hprev.size() == 1)
      skip_over_pull(r, PortT(x, 0), r->connection(hprev[0]).from());
    else if (hnext.size() == 1)
      skip_over_push(r, PortT(x, 0), r->connection(hnext[0]).to());

    x->kill();
  }
}

static bool
remove_redundant_schedulers(RouterT *r, ElementClassT *t,
			    bool config_eq_ninputs, ErrorHandler *errh)
{
  if (!t)
    return false;
  ElementClassT *idlet = r->get_type("Idle");

  bool changed = false;
  for (RouterT::type_iterator x = r->first_element(t); x; x++) {
    assert(x->type() == t);
    if (x->noutputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to `%s'", x->declaration().cc());
      continue;
    }
    
    Vector<int> hprev;
    Vector<String> args;
    r->find_connection_vector_to(x, hprev);
    // check configuration string if we need to
    if (config_eq_ninputs) {
      cp_argvec(x->configuration(), args);
      if (args.size() != hprev.size())
	continue;
    }
    
    for (int p = 0; p < hprev.size(); p++)
      if (hprev[p] == -1 || (hprev[p] >= 0 && r->connection(hprev[p]).from_elt()->type() == idlet)) {
	// remove that scheduler port
	// check configuration first
	if (config_eq_ninputs) {
	  for (int pp = p + 1; pp < hprev.size(); pp++)
	    args[pp-1] = args[pp];
	  args.pop_back();
	  x->configuration() = cp_unargvec(args);
	}

	// now do connections
	int bad_connection = hprev[p];
	for (int pp = p + 1; pp < hprev.size(); pp++) {
	  r->change_connection_to(hprev[pp], PortT(x, pp - 1));
	  hprev[pp - 1] = hprev[pp];
	}
	if (bad_connection >= 0)
	  r->kill_connection(bad_connection);
	hprev.pop_back();
	p--;
      }
    
    if (hprev.size() == 1) {
      if (verbose)
	errh->lerror(x->landmark(), "removing redundant scheduler `%s'", x->declaration().cc());
      skip_over_pull(r, PortT(x, 0), r->connection(hprev[0]).from());
      x->kill();
      changed = true;
    }

    // save number of inputs so we don't attach new Idles
    element_ninputs.insert(x->name(), hprev.size());
  }

  return changed;
}

static bool
remove_redundant_tee_ports(RouterT *r, ElementClassT *t, bool is_pull_tee,
			   ErrorHandler *errh)
{
  if (!t)
    return false;
  ElementClassT *idlet = r->get_type("Idle");

  bool changed = false;
  for (RouterT::type_iterator x = r->first_element(t); x; x++) {
    assert(x->type() == t);
    if (x->ninputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to `%s'", x->declaration().cc());
      continue;
    }
    
    Vector<int> hnext;
    Vector<String> args;
    r->find_connection_vector_from(x, hnext);
    
    for (int p = hnext.size() - 1; p >= (is_pull_tee ? 1 : 0); p--)
      if (hnext[p] == -1 || (hnext[p] >= 0 && r->connection(hnext[p]).from_elt()->type() == idlet)) {
	// remove that tee port
	int bad_connection = hnext[p];
	for (int pp = p + 1; pp < hnext.size(); pp++) {
	  r->change_connection_from(hnext[pp], PortT(x, pp - 1));
	  hnext[pp - 1] = hnext[pp];
	}
	if (bad_connection >= 0)
	  r->kill_connection(bad_connection);
	hnext.pop_back();
      }
    
    if (hnext.size() == 1) {
      if (verbose)
	errh->lerror(x->landmark(), "removing redundant tee `%s'", x->declaration().cc());
      if (is_pull_tee) {
	Vector<int> hprev;
	r->find_connection_vector_to(x, hprev);
	skip_over_pull(r, PortT(x, 0), r->connection(hprev[0]).from());
      } else
	skip_over_push(r, PortT(x, 0), r->connection(hnext[0]).to());
      x->kill();
      changed = true;
    }

    // save number of outputs so we don't attach new Idles
    element_noutputs.insert(x->name(), hnext.size());
    x->configuration() = String(hnext.size());
  }

  return changed;
}

static void
find_live_elements(/*const*/ RouterT *r, const char *filename,
		   ElementMap &full_elementmap, int driver,
		   bool indifferent,
		   Bitvector &live_elements, ErrorHandler *errh)
{
  if (!indifferent && !full_elementmap.driver_compatible(r, driver)) {
    errh->error("%s: configuration incompatible with %s driver", filename, Driver::name(driver));
    return;
  }

  full_elementmap.set_driver(driver);

  // get processing
  ProcessingT processing(r, &full_elementmap, errh);
  // ... it will report errors as required

  Bitvector sources(r->nelements(), false);
  Bitvector sinks(r->nelements(), false);
  Bitvector dead(r->nelements(), false);

  // find initial sources and sinks
  for (RouterT::live_iterator x = r->first_live_element(); x; x++) {
    int nin = x->ninputs();
    int nout = x->noutputs();
    int source_flag = x->type()->traits().flag_value('S');
    int ei = x->idx();

    if (source_flag == 0) {	// neither source nor sink
      dead[ei] = true;
      continue;
    } else if (source_flag == 1) { // source
      sources[ei] = true;
      if (verbose)
	errh->lmessage(x->landmark(), "`%s' is source", x->declaration().cc());
      continue;
    } else if (source_flag == 2) { // sink
      sinks[ei] = true;
      if (verbose)
	errh->lmessage(x->landmark(), "`%s' is sink", x->declaration().cc());
      continue;
    } else if (source_flag == 3) { // source and sink
      sources[ei] = sinks[ei] = true;
      if (verbose)
	errh->lmessage(x->landmark(), "`%s' is source and sink", x->declaration().cc());
      continue;
    } else if (source_flag > 0)
      errh->lwarning(x->landmark(), "`%s' has strange source/sink flag value %d", x->declaration().cc(), source_flag);

    // if no source/sink flags, make an educated guess
    if (nin == 0) {
      for (int p = 0; p < nout; p++)
	if (processing.output_is_push(ei, p)) {
	  sources[ei] = true;
	  if (verbose)
	    errh->lmessage(x->landmark(), "assuming `%s' is source", x->declaration().cc());
	  break;
	}
    }
    if (nout == 0) {
      for (int p = 0; p < nin; p++)
	if (processing.input_is_pull(ei, p)) {
	  sinks[ei] = true;
	  if (verbose)
	    errh->lmessage(x->landmark(), "assuming `%s' is sink", x->declaration().cc());
	  break;
	}
    }
  }

  int nh = r->nconnections();
  const Vector<ConnectionT> &conn = r->connections();
  
  // spread sources
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < nh; i++) {
      int f = conn[i].from_idx(), t = conn[i].to_idx();
      if (f >= 0 && !sources[t] && sources[f] && !dead[t])
	sources[t] = changed = true;
    }
  }
  
  // spread sinks
  changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < nh; i++) {
      int f = conn[i].from_idx(), t = conn[i].to_idx();
      if (f >= 0 && !sinks[f] && sinks[t] && !dead[f])
	sinks[f] = changed = true;
    }
  }

  // live = (sources & sinks) | independently_live
  live_elements = sources & sinks;

  // find independently live elements
  for (RouterT::live_iterator x = r->first_live_element(); x; x++)
    if (!live_elements[x->idx()]) {
      int ei = x->idx();
      int live_flag = x->type()->traits().flag_value('L');
      if (live_flag == 0)	// not live
	continue;
      else if (live_flag == 1) { // live
	live_elements[ei] = true;
	continue;
      } else if (live_flag > 0)
	errh->lwarning(x->landmark(), "`%s' has strange live flag value %d", x->declaration().cc(), live_flag);

      // if no live flag, make an educated guess
      if (x->ninputs() == 0 && x->noutputs() == 0) {
	live_elements[ei] = true;
	if (verbose)
	  errh->lmessage(x->landmark(), "assuming `%s' is live", x->declaration().cc());
      }
    }
}

static void
replace_blank_ports(RouterT *r)
{
  ElementT *idle = 0;
  int idle_next_in = 0, idle_next_out = 0;
  for (RouterT::live_iterator x = r->first_live_element(); x; x++) {
    Vector<int> connv;

    r->find_connection_vector_to(x, connv);
    connv.resize(element_ninputs[x->name()], -1);
    for (int p = 0; p < connv.size(); p++)
      if (connv[p] == -1) {	// unconnected port
	if (!idle)
	  idle = r->add_anon_element(r->get_type("Idle"), "", "<click-undead>");
	r->add_connection(idle, idle_next_out++, x, p);
      }

    r->find_connection_vector_from(x, connv);
    connv.resize(element_noutputs[x->name()], -1);
    for (int p = 0; p < connv.size(); p++)
      if (connv[p] == -1) {	// unconnected port
	if (!idle)
	  idle = r->add_anon_element(r->get_type("Idle"), "", "<click-undead>");
	r->add_connection(x, p, idle, idle_next_in++);
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
  CLICK_DEFAULT_PROVIDES;

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
      printf("click-undead (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
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
  if (r)
    r->flatten(default_errh);
  if (!r || default_errh->nerrors() > 0)
    exit(1);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      default_errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find and parse `elementmap'
  ElementMap *emap = ElementMap::default_map();
  emap->parse_all_files(r, CLICK_SHAREDIR, p_errh);

  // check configuration for driver indifference
  bool indifferent = emap->driver_indifferent(r, Driver::ALLMASK, default_errh);
  if (indifferent) {
    if (check_kernel < 0 && check_userlevel < 0)
      // only bother to check one of them
      check_kernel = 0;
  } else {
    if (check_kernel < 0 && check_userlevel < 0) {
      check_kernel = emap->driver_compatible(r, Driver::LINUXMODULE);
      check_userlevel = emap->driver_compatible(r, Driver::USERLEVEL);
    }
  }

  // save numbers of inputs and outputs for later
  save_element_nports(r);

  // remove elements who make static routing decisions
  remove_static_switches(r, default_errh);
  remove_static_pull_switches(r, default_errh);
  remove_nulls(r, r->try_type("Null"), default_errh);

  // remove dead elements to improve processing checking
  r->remove_dead_elements();
  
  // find live elements in the drivers
  Bitvector kernel_vec, user_vec;
  if (check_kernel > 0)
    find_live_elements(r, router_file, *emap,
		       Driver::LINUXMODULE, indifferent,
		       kernel_vec, default_errh);
  if (check_userlevel > 0)
    find_live_elements(r, router_file, *emap,
		       Driver::USERLEVEL, indifferent,
		       user_vec, default_errh);

  // an element is live if it's live in either driver
  Bitvector live_vec = kernel_vec | user_vec;
  if (!live_vec.size() && r->nelements())
    exit(1);

  // remove Idles
  for (int i = 0; i < r->nelements(); i++)
    if (!live_vec[i]) {
      ElementT *e = r->element(i);
      if (verbose)
	default_errh->lmessage(e->landmark(), "removing `%s'", e->declaration().cc());
      e->kill();
    }
  
  // remove dead connections (not elements yet: keep indexes in 'processing'
  // the same)
  r->kill_bad_connections();
  r->check();
  
  // remove redundant schedulers
  while (1) {
    int nchanges = 0;
    nchanges += remove_redundant_schedulers(r, r->try_type("RoundRobinSched"), false, default_errh);
    nchanges += remove_redundant_schedulers(r, r->try_type("PrioSched"), false, default_errh);
    nchanges += remove_redundant_schedulers(r, r->try_type("StrideSched"), true, default_errh);
    nchanges += remove_redundant_tee_ports(r, r->try_type("Tee"), false, default_errh);
    nchanges += remove_redundant_tee_ports(r, r->try_type("PullTee"), true, default_errh);
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
