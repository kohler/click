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
#include <click/args.hh>
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
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define KERNEL_OPT		306
#define USERLEVEL_OPT		307
#define CONFIG_OPT		308
#define VERBOSE_OPT		310

static const Clp_Option options[] = {
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static bool verbose;

static HashTable<String, int> element_ninputs(-1);
static HashTable<String, int> element_noutputs(-1);

static ElementClassT *idlet;

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
'Click-undead' transforms a router configuration by removing dead code. This\n\
includes any elements that are not connected both to a packet source and to a\n\
packet sink. It also removes redundant elements, such as Null and StaticSwitch.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -e, --expression EXPR         Use EXPR as router configuration.\n\
  -o, --output FILE             Write output to FILE.\n\
  -k, --kernel                  Configuration is for Linux kernel driver.\n\
  -u, --user                    Configuration is for user-level driver.\n\
  -c, --config                  Write new configuration only.\n\
  -V, --verbose                 Print debugging information.\n\
  -C, --clickpath PATH          Use PATH for CLICKPATH.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
}


static void
save_element_nports(RouterT *r)
{
  for (RouterT::iterator x = r->begin_elements(); x; x++) {
    element_ninputs.set(x->name(), x->ninputs());
    element_noutputs.set(x->name(), x->noutputs());
  }
}

static void
remove_static_switches(RouterT *r, ErrorHandler *errh)
{
  ElementClassT *t = ElementClassT::base_type("StaticSwitch");

  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    assert(x->type() == t);

    String config = cp_uncomment(x->configuration());
    int val;
    if (!IntArg().parse(config, val)) {
      errh->lerror(x->landmark(), "%s: bad configuration %<StaticSwitch(%s)%>", x->name_c_str(), config.c_str());
      val = -1;
    }

    Vector<RouterT::conn_iterator> connv_out;
    r->find_connection_vector_from(x.get(), connv_out);
    for (int i = 0; i < connv_out.size(); i++)
      if (!connv_out[i].is_back()) {
	errh->lerror(x->landmark(), "odd connections from %<%s%>", x->declaration().c_str());
	break;
      }

    ElementT *idle = r->add_anon_element(idlet, "", LandmarkT("<click-undead>"));
    int idle_in = 0, idle_out = 0;

    PortT jump_hook;
    if (val < 0 || val >= x->noutputs() || !connv_out[val].is_back())
      jump_hook = PortT(idle, idle_in++);
    else
      jump_hook = connv_out[val]->to();

    for (RouterT::conn_iterator xit = r->find_connections_to(x.get(), 0);
	 xit != r->end_connections(); )
	xit = r->change_connection_to(xit, jump_hook);

    for (int j = 0; j < connv_out.size(); j++)
      if (j != val)
	r->change_connection_from(connv_out[j], PortT(idle, idle_out++));

    x->kill();
  }
}

static void
remove_static_pull_switches(RouterT *r, ErrorHandler *errh)
{
  ElementClassT *t = ElementClassT::base_type("StaticPullSwitch");

  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    assert(x->type() == t);

    String config = cp_uncomment(x->configuration());
    int val;
    if (!IntArg().parse(config, val)) {
      errh->lerror(x->landmark(), "%s: bad configuration 'StaticSwitch(%s)'", x->name_c_str(), config.c_str());
      val = -1;
    }

    Vector<RouterT::conn_iterator> connv_in;
    r->find_connection_vector_to(x.get(), connv_in);
    for (int i = 0; i < connv_in.size(); i++)
      if (!connv_in[i].is_back()) {
	errh->lerror(x->landmark(), "odd connections to %<%s%>", x->declaration().c_str());
	break;
      }

    ElementT *idle = r->add_anon_element(idlet, "", LandmarkT("<click-undead>"));
    int idle_in = 0, idle_out = 0;

    PortT jump_hook;
    if (val < 0 || val >= x->ninputs() || !connv_in[val].is_back())
      jump_hook = PortT(idle, idle_out++);
    else
      jump_hook = connv_in[val]->from();

    for (RouterT::conn_iterator xit = r->find_connections_from(x.get(), 0);
	 xit != r->end_connections(); )
	xit = r->change_connection_from(xit, jump_hook);

    for (int j = 0; j < connv_in.size(); j++)
      if (j != val)
	r->change_connection_to(connv_in[j], PortT(idle, idle_in++));

    x->kill();
  }
}

static void
skip_over_push(RouterT *r, const PortT &old_to, const PortT &new_to)
{
    RouterT::conn_iterator it = r->find_connections_to(old_to);
    while (it)
	it = r->change_connection_to(it, new_to);
}

static void
skip_over_pull(RouterT *r, const PortT &old_from, const PortT &new_from)
{
    RouterT::conn_iterator it = r->find_connections_from(old_from);
    while (it)
	it = r->change_connection_from(it, new_from);
}

static void
remove_nulls(RouterT *r, ElementClassT *t, ErrorHandler *errh)
{
  if (!t)
    return;

  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    assert(x->type() == t);
    if (x->ninputs() != 1 || x->noutputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to %<%s%>", x->declaration().c_str());
      continue;
    }

    RouterT::conn_iterator previt = r->find_connections_to(PortT(x.get(), 0));
    RouterT::conn_iterator nextit = r->find_connections_from(PortT(x.get(), 0));
    if (previt && !previt.is_back() && nextit && !nextit.is_back())
      errh->lwarning(x->landmark(), "odd connections to %<%s%>", x->declaration().c_str());
    else if (previt.is_back())
      skip_over_pull(r, PortT(x.get(), 0), previt->from());
    else if (nextit.is_back())
      skip_over_push(r, PortT(x.get(), 0), nextit->to());

    x->kill();
  }
}

static bool
remove_redundant_schedulers(RouterT *r, ElementClassT *t,
			    bool config_eq_ninputs, ErrorHandler *errh)
{
  if (!t)
    return false;

  bool changed = false;
  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    assert(x->type() == t);
    if (x->noutputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to %<%s%>", x->declaration().c_str());
      continue;
    }

    Vector<RouterT::conn_iterator> hprev;
    Vector<String> args;
    r->find_connection_vector_to(x.get(), hprev);
    // check configuration string if we need to
    if (config_eq_ninputs) {
      cp_argvec(x->configuration(), args);
      if (args.size() != hprev.size())
	continue;
    }

    for (int p = 0; p < hprev.size(); p++)
      if (!hprev[p]
	  || (hprev[p] && hprev[p]->from_element()->type() == idlet)) {
	// remove that scheduler port
	// check configuration first
	if (config_eq_ninputs) {
	  for (int pp = p + 1; pp < hprev.size(); pp++)
	    args[pp-1] = args[pp];
	  args.pop_back();
	  x->set_configuration(cp_unargvec(args));
	}

	// now do connections
	if (hprev[p])
	    r->erase(hprev[p]);
	for (int pp = p + 1; pp < hprev.size(); pp++) {
	  r->change_connection_to(hprev[pp], PortT(x.get(), pp - 1));
	  hprev[pp - 1] = hprev[pp];
	}
	hprev.pop_back();
	p--;
      }

    if (hprev.size() == 1) {
      if (verbose)
	errh->lerror(x->landmark(), "removing redundant scheduler %<%s%>", x->declaration().c_str());
      skip_over_pull(r, PortT(x.get(), 0), hprev[0]->from());
      x->kill();
      changed = true;
    }

    // save number of inputs so we don't attach new Idles
    element_ninputs.set(x->name(), hprev.size());
  }

  return changed;
}

static bool
remove_redundant_tee_ports(RouterT *r, ElementClassT *t, bool is_pull_tee,
			   ErrorHandler *errh)
{
  if (!t)
    return false;

  bool changed = false;
  for (RouterT::type_iterator x = r->begin_elements(t); x; x++) {
    assert(x->type() == t);
    if (x->ninputs() != 1) {
      errh->lwarning(x->landmark(), "odd connections to %<%s%>", x->declaration().c_str());
      continue;
    }

    Vector<RouterT::conn_iterator> hnext;
    Vector<String> args;
    r->find_connection_vector_from(x.get(), hnext);

    for (int p = hnext.size() - 1; p >= (is_pull_tee ? 1 : 0); p--)
      if (!hnext[p] || (hnext[p].is_back() && hnext[p]->from_element()->type() == idlet)) {
	// remove that tee port
	if (hnext[p])
	    r->erase(hnext[p]);
	for (int pp = p + 1; pp < hnext.size(); pp++) {
	  r->change_connection_from(hnext[pp], PortT(x.get(), pp - 1));
	  hnext[pp - 1] = hnext[pp];
	}
	hnext.pop_back();
      }

    if (hnext.size() == 1) {
      if (verbose)
	errh->lerror(x->landmark(), "removing redundant tee %<%s%>", x->declaration().c_str());
      if (is_pull_tee) {
	Vector<RouterT::conn_iterator> hprev;
	r->find_connection_vector_to(x.get(), hprev);
	skip_over_pull(r, PortT(x.get(), 0), hprev[0]->from());
      } else
	skip_over_push(r, PortT(x.get(), 0), hnext[0]->to());
      x->kill();
      changed = true;
    }

    // save number of outputs so we don't attach new Idles
    element_noutputs.set(x->name(), hnext.size());
    x->set_configuration(String(hnext.size()));
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

  // reset flow codes for all elements with "L2" flag
  {
      HashTable<ElementClassT *, int> classmap;
      for (RouterT::iterator it = r->begin_elements(); it; ++it) {
	  int &used = classmap[it->type()];
	  if (!used) {
	      used = 1;
	      if (it->type()->traits().flag_value("L") == 2)
		  it->type()->force_traits().flow_code = "";
	  }
      }
  }

  // get processing
  ProcessingT processing(r, &full_elementmap, errh);
  // ... it will report errors as required

  Bitvector sources(r->nelements(), false),
      sinks(r->nelements(), false),
      connected_to_sources(processing.ninput_pidx(), false),
      connected_to_sinks(processing.noutput_pidx(), false);

  // find initial sources and sinks
  for (RouterT::iterator x = r->begin_elements(); x; x++) {
    int nin = x->ninputs();
    int nout = x->noutputs();
    int source_flag = x->type()->traits().flag_value("S");
    const char *assuming = "";

    if (source_flag < 0 || source_flag > 3) {
	// if no source/sink flags, make an educated guess
	if (source_flag > 0)
	    errh->lwarning(x->landmark(), "%<%s%> has strange source/sink flag value %d", x->declaration().c_str(), source_flag);
	source_flag = (nin ? 0 : 1) | (nout ? 0 : 2);
	assuming = "assuming ";
    }

    if (source_flag & 1) {	   // source
	if (verbose)
	    errh->lmessage(x->landmark(), "%s%<%s%> is source%s", assuming, x->declaration().c_str(), (source_flag & 2 ? " and sink" : ""));
	sources[x->eindex()] = true;
	processing.follow_connections(PortT(x.get(), -1), true, connected_to_sources);
    }
    if (source_flag & 2) {	   // sink
	if (verbose && !(source_flag & 1))
	    errh->lmessage(x->landmark(), "%s%<%s%> is sink", assuming, x->declaration().c_str());
	sinks[x->eindex()] = true;
	processing.follow_connections(PortT(x.get(), -1), false, connected_to_sinks);
    }
  }

  processing.follow_reachable(connected_to_sources, false, true, errh);
  processing.follow_reachable(connected_to_sinks, true, false, errh);

  for (RouterT::iterator x = r->begin_elements(); x; x++) {
      if (x->type() != idlet && !sources[x->eindex()]) {
	  int pidx0 = processing.input_pidx(x->eindex());
	  for (int p = 0; p < x->ninputs(); ++p)
	      if (connected_to_sources[pidx0 + p]) {
		  sources[x->eindex()] = true;
		  break;
	      }
      }
      if (x->type() != idlet && !sinks[x->eindex()]) {
	  int pidx0 = processing.output_pidx(x->eindex());
	  for (int p = 0; p < x->noutputs(); ++p)
	      if (connected_to_sinks[pidx0 + p]) {
		  sinks[x->eindex()] = true;
		  break;
	      }
      }
  }

  // live = (sources & sinks) | independently_live
  live_elements = sources & sinks;

  // find independently live elements
  for (RouterT::iterator x = r->begin_elements(); x; x++)
    if (!live_elements[x->eindex()]) {
      int ei = x->eindex();
      int live_flag = x->type()->traits().flag_value("L");
      if (live_flag == 0)	// not live
	continue;
      else if (live_flag == 1) { // live
	live_elements[ei] = true;
	continue;
      } else if (live_flag > 2)
	errh->lwarning(x->landmark(), "%<%s%> has strange live flag value %d", x->declaration().c_str(), live_flag);

      // if no live flag, make an educated guess
      if (x->ninputs() == 0 && x->noutputs() == 0) {
	live_elements[ei] = true;
	if (verbose)
	  errh->lmessage(x->landmark(), "assuming %<%s%> is live", x->declaration().c_str());
      }
    }
}

static void
replace_blank_ports(RouterT *r)
{
  ElementT *idle = r->add_anon_element(idlet, "", LandmarkT("<click-undead>"));
  int idle_next_in = 0, idle_next_out = 0;
  for (RouterT::iterator x = r->begin_elements(); x; x++) {
    Vector<RouterT::conn_iterator> connv;
    int nin = element_ninputs[x->name()], nout = element_noutputs[x->name()];

    r->find_connection_vector_to(x.get(), connv);
    if (nin >= 0)
      connv.resize(nin);
    for (int p = 0; p < connv.size(); p++)
      if (!connv[p]) {	// unconnected port
	if (!idle)
	  idle = r->add_anon_element(idlet, "", LandmarkT("<click-undead>"));
	r->add_connection(idle, idle_next_out++, x.get(), p);
      }

    r->find_connection_vector_from(x.get(), connv);
    if (nout >= 0)
      connv.resize(nout);
    for (int p = 0; p < connv.size(); p++)
      if (!connv[p]) {	// unconnected port
	if (!idle)
	  idle = r->add_anon_element(idlet, "", LandmarkT("<click-undead>"));
	r->add_connection(x.get(), p, idle, idle_next_in++);
      }
  }
  if (!idle_next_in && !idle_next_out)
      idle->kill();
}


int
main(int argc, char **argv)
{
  click_static_initialize();
  CLICK_DEFAULT_PROVIDES;
  ErrorHandler *default_errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(default_errh, "click-undead: ");

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  bool file_is_expr = false;
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
      set_clickpath(clp->vstr);
      break;

     case ROUTER_OPT:
     case EXPRESSION_OPT:
     router_file:
      if (router_file) {
	p_errh->error("router configuration specified twice");
	goto bad_option;
      }
      router_file = clp->vstr;
      file_is_expr = (opt == EXPRESSION_OPT);
      break;

     case Clp_NotOption:
      if (!click_maybe_define(clp->vstr, p_errh))
	  goto router_file;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->vstr;
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
  RouterT *r = read_router(router_file, file_is_expr, default_errh);
  if (r)
    r->flatten(default_errh);
  if (!r || default_errh->nerrors() > 0)
    exit(1);
  if (file_is_expr)
    router_file = "config";
  else if (!router_file || strcmp(router_file, "-") == 0)
    router_file = "<stdin>";

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      default_errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // find and parse 'elementmap'
  ElementMap *emap = ElementMap::default_map();
  emap->parse_all_files(r, CLICK_DATADIR, p_errh);

  // check configuration for driver indifference
  bool indifferent = emap->driver_indifferent(r, Driver::ALLMASK);
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

  // set types
  idlet = ElementClassT::base_type("Idle");

  // remove elements who make static routing decisions
  remove_static_switches(r, default_errh);
  remove_static_pull_switches(r, default_errh);
  remove_nulls(r, ElementClassT::base_type("Null"), default_errh);

  // remove dead elements to improve processing checking
  r->remove_dead_elements();

  // find live elements in the drivers
  Bitvector kernel_vec, user_vec;
  if (check_kernel != 0)
    find_live_elements(r, router_file, *emap,
		       Driver::LINUXMODULE, indifferent,
		       kernel_vec, default_errh);
  if (check_userlevel != 0)
    find_live_elements(r, router_file, *emap,
		       Driver::USERLEVEL, indifferent,
		       user_vec, default_errh);

  // an element is live if it's live in either driver
  Bitvector live_vec = kernel_vec | user_vec;
  if (live_vec.zero() && r->nelements()) {
    default_errh->warning("no live elements in configuration");
    exit(1);
  }

  // remove Idles
  for (int i = 0; i < r->nelements(); i++)
    if (!live_vec[i]) {
      ElementT *e = r->element(i);
      if (verbose)
	default_errh->lmessage(e->landmark(), "removing %<%s%>", e->declaration().c_str());
      e->kill();
    }

  // remove dead connections (not elements yet: keep indexes in 'processing'
  // the same)
  r->kill_bad_connections();
  r->check();

  // remove redundant schedulers
  while (1) {
      int nchanges = 0;
      nchanges += remove_redundant_schedulers(r, ElementClassT::base_type("RoundRobinSched"), false, default_errh);
      nchanges += remove_redundant_schedulers(r, ElementClassT::base_type("PrioSched"), false, default_errh);
      nchanges += remove_redundant_schedulers(r, ElementClassT::base_type("StrideSched"), true, default_errh);
      nchanges += remove_redundant_tee_ports(r, ElementClassT::base_type("Tee"), false, default_errh);
      nchanges += remove_redundant_tee_ports(r, ElementClassT::base_type("PullTee"), true, default_errh);
      if (!nchanges)
	  break;
  }

  // hook up blanked-out live ports to a new Idle
  replace_blank_ports(r);

  // NOW remove bad elements
  r->remove_dead_elements();

  if (config_only) {
    String config = r->configuration_string();
    ignore_result(fwrite(config.data(), 1, config.length(), outf));
  } else
    write_router_file(r, outf, default_errh);

  exit(0);
}
