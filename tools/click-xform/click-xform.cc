/*
 * click-xform.cc -- pattern-based optimizer for Click configurations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/variableenv.hh>
#include <click/driver.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "adjacency.hh"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

bool match_config(const String &, const String &, HashMap<String, String> &);
// TODO: allow some special pports to be unconnected

class Matcher {

  RouterT *_pat;
  AdjacencyMatrix *_pat_m;
  RouterT *_body;
  AdjacencyMatrix *_body_m;
  int _patid;

  int _pat_input_idx;
  int _pat_output_idx;
  
  Vector<int> _match;
  Vector<int> _back_match;
  HashMap<String, String> _defs;

  Vector<HookupI> _to_pp_from;
  Vector<HookupI> _to_pp_to;
  Vector<HookupI> _from_pp_from;
  Vector<HookupI> _from_pp_to;

 public:

  Matcher(RouterT *, AdjacencyMatrix *, RouterT *, AdjacencyMatrix *, int, ErrorHandler *);
  ~Matcher();

  bool check_into(const HookupI &, const HookupI &);
  bool check_out_of(const HookupI &, const HookupI &);

  bool check_match();
  bool next_match();

  void replace_config(String &) const;
  void replace(RouterT *, const String &, const String &, ErrorHandler *);
  
};


Matcher::Matcher(RouterT *pat, AdjacencyMatrix *pat_m,
		 RouterT *body, AdjacencyMatrix *body_m,
		 int patid, ErrorHandler *errh)
  : _pat(pat), _pat_m(pat_m), _body(body), _body_m(body_m), _patid(patid),
    _pat_input_idx(-1), _pat_output_idx(-1)
{
  // check tunnel situation
  for (int i = 0; i < _pat->nelements(); i++) {
    ElementT &fac = *(_pat->element(i));
    if (!fac.tunnel())
      continue;
    else if (fac.tunnel_connected())
      errh->lerror(fac.landmark(), "pattern has active connection tunnels");
    else if (fac.name() == "input" && _pat_input_idx < 0)
      _pat_input_idx = i;
    else if (fac.name() == "output" && _pat_output_idx < 0)
      _pat_output_idx = i;
    else
      errh->lerror(fac.landmark(), "connection tunnel with funny name `%s'", fac.name_cc());
  }
}

Matcher::~Matcher()
{
}

bool
Matcher::check_into(const HookupI &houtside, const HookupI &hinside)
{
  const Vector<ConnectionT> &pconn = _pat->connections();
  HookupI phinside(_back_match[hinside.idx], hinside.port);
  HookupI success(_pat->nelements(), 0);
  // now look for matches
  for (int i = 0; i < pconn.size(); i++)
    if (pconn[i].to() == phinside && pconn[i].from_idx() == _pat_input_idx
	&& pconn[i].from() < success) {
      Vector<HookupI> pfrom_phf, from_houtside;
      // check that it's valid: all connections from tunnels are covered
      // in body
      _pat->find_connections_from(pconn[i].from(), pfrom_phf);
      _body->find_connections_from(houtside, from_houtside);
      for (int j = 0; j < pfrom_phf.size(); j++) {
	HookupI want(_match[pfrom_phf[j].idx], pfrom_phf[j].port);
	if (want.index_in(from_houtside) < 0)
	  goto no_match;
      }
      // success: save it
      success = pconn[i].from();
     no_match: ;
    }
  // if succeeded, save it
  if (success.idx < _pat->nelements()) {
    _to_pp_from.push_back(houtside);
    _to_pp_to.push_back(success);
    return true;
  } else
    return false;
}

bool
Matcher::check_out_of(const HookupI &hinside, const HookupI &houtside)
{
  const Vector<ConnectionT> &pconn = _pat->connections();
  HookupI phinside(_back_match[hinside.idx], hinside.port);
  HookupI success(_pat->nelements(), 0);
  // now look for matches
  for (int i = 0; i < pconn.size(); i++)
    if (pconn[i].from() == phinside && pconn[i].to_idx() == _pat_output_idx
	&& pconn[i].to() < success) {
      Vector<HookupI> pto_pht, to_houtside;
      // check that it's valid: all connections to tunnels are covered
      // in body
      _pat->find_connections_to(pconn[i].to(), pto_pht);
      _body->find_connections_to(houtside, to_houtside);
      for (int j = 0; j < pto_pht.size(); j++) {
	HookupI want(_match[pto_pht[j].idx], pto_pht[j].port);
	if (want.index_in(to_houtside) < 0)
	  goto no_match;
      }
      // success: save it
      success = pconn[i].to();
     no_match: ;
    }
  // if succeeded, save it
  if (success.idx < _pat->nelements()) {
    _from_pp_from.push_back(success);
    _from_pp_to.push_back(houtside);
    return true;
  } else
    return false;
}

bool
Matcher::check_match()
{
  _from_pp_from.clear();
  _from_pp_to.clear();
  _to_pp_from.clear();
  _to_pp_to.clear();
  _defs.clear();

  // check configurations
  //fprintf(stderr, "CONF\n");
  for (int i = 0; i < _match.size(); i++)
    if (_match[i] >= 0) {
      if (!match_config(_pat->econfiguration(i), _body->econfiguration(_match[i]), _defs))
	return false;
    }
  
  int bnf = _body->nelements();
  _back_match.assign(bnf, -1);
  bool all_previous_match = true;
  for (int i = 0; i < _match.size(); i++) {
    int j = _match[i];
    if (j >= 0) {
      _back_match[j] = i;
      if (_body->eflags(j) != _patid)	// comes from replacement of same pat
	all_previous_match = false;
    }
  }
  // don't allow match if it consists entirely of elements previously replaced
  // by this pattern
  if (all_previous_match)
    return false;

  // find the pattern ports any cross-pattern jumps correspond to
  //fprintf(stderr, "XPJ\n");
  const Vector<ConnectionT> &conn = _body->connections();
  int nhook = conn.size();

  for (int i = 0; i < nhook; i++) {
    if (conn[i].dead())
      continue;
    const HookupI &hf = conn[i].from(), &ht = conn[i].to();
    int pf = _back_match[hf.idx], pt = _back_match[ht.idx];
    if (pf >= 0 && pt >= 0) {
      if (!_pat->has_connection(HookupI(pf, hf.port), HookupI(pt, ht.port)))
	return false;
    } else if (pf < 0 && pt >= 0) {
      if (!check_into(hf, ht))
	return false;
    } else if (pf >= 0 && pt < 0) {
      if (!check_out_of(hf, ht))
	return false;
    }
  }

  // check for unconnected tunnels in the pattern
  //fprintf(stderr, "UNC\n");
  const Vector<ConnectionT> &pconn = _pat->connections();
  for (int i = 0; i < pconn.size(); i++) {
    if (pconn[i].from_idx() == _pat_input_idx
	&& pconn[i].from().index_in(_to_pp_to) < 0)
      return false;
    if (pconn[i].to_idx() == _pat_output_idx
	&& pconn[i].to().index_in(_from_pp_from) < 0)
      return false;
  }

  //fprintf(stderr, "  YES\n");
  return true;
}

bool
Matcher::next_match()
{
  while (_pat_m->next_subgraph_isomorphism(_body_m, _match)) {
    if (check_subgraph_isomorphism(_pat, _body, _match)
	&& check_match())
      return true;
  }
  return false;
}



static String
uniqueify_prefix(const String &base_prefix, RouterT *r)
{
  // speed up uniqueification on the same prefix
  static HashMap<String, int> *last_uniqueifier;
  if (!last_uniqueifier)
    last_uniqueifier = new HashMap<String, int>(1);
  int count = (*last_uniqueifier)[base_prefix];
  
  while (1) {
    String prefix = base_prefix + "@" + String(count);
    count++;
    
    // look for things starting with that name
    int plen = prefix.length();
    for (int i = 0; i < r->nelements(); i++) {
      const String &n = r->ename(i);
      if (n.length() > plen + 1 && n.substring(0, plen) == prefix
	  && n[plen] == '/')
	goto failed;
    }

    last_uniqueifier->insert(base_prefix, count);
    return prefix;

   failed: ;
  }
}

void
Matcher::replace_config(String &configuration) const
{
  Vector<String> confvec;
  cp_argvec(configuration, confvec);
  
  bool changed = false;
  for (int i = 0; i < confvec.size(); i++) {
    if (confvec[i].length() <= 1 || confvec[i][0] != '$')
      continue;
    if (String *vp = _defs.findp(confvec[i])) {
      confvec[i] = *vp;
      changed = true;
    }
  }

  if (changed)
    configuration = cp_unargvec(confvec);
}

void
Matcher::replace(RouterT *replacement, const String &try_prefix,
		 const String &landmark, ErrorHandler *errh)
{
  //fprintf(stderr, "replace...\n");
  String prefix = uniqueify_prefix(try_prefix, _body);

  // free old elements
  Vector<int> changed_elements;
  Vector<String> old_names;
  for (int i = 0; i < _match.size(); i++)
    if (_match[i] >= 0) {
      changed_elements.push_back(_match[i]);
      old_names.push_back(_body->ename(_match[i]));
      _body->free_element(_match[i]);
    } else
      old_names.push_back(String());
  
  // add replacement
  // collect new element indices in `changed_elements'
  _body->set_new_eindex_collector(&changed_elements);

  // make element named `prefix'
  int new_eindex = _body->get_eindex(prefix, ElementClassT::tunnel_type(), String(), landmark);

  // expand 'replacement' into '_body'; need crap compound element
  Vector<String> crap_args;
  CompoundElementClassT comp("<replacement>", replacement);
  comp.complex_expand_element(_body, new_eindex, String(), crap_args, _body, VariableEnvironment(), errh);

  // mark replacement
  for (int i = 0; i < changed_elements.size(); i++) {
    int j = changed_elements[i];
    if (_body->element(j)->dead())
      continue;
    _body->element(j)->flags = _patid;
    replace_config(_body->econfiguration(j));
  }

  // save old element name if matched element and some replacement element
  // have the same name
  for (int i = 0; i < _match.size(); i++)
    if (_match[i] >= 0) {
      String n = _pat->ename(i);
      int new_index = _body->eindex(prefix + "/" + n);
      if (new_index >= 0) 
	_body->change_ename(new_index, old_names[i]);
    }

  // find input and output, add connections to tunnels
  int new_pp = _body->eindex(prefix);
  for (int i = 0; i < _to_pp_from.size(); i++)
    _body->add_connection(_to_pp_from[i], HookupI(new_pp, _to_pp_to[i].port),
			  landmark);
  for (int i = 0; i < _from_pp_from.size(); i++)
    _body->add_connection(HookupI(new_pp, _from_pp_from[i].port),
			  _from_pp_to[i], landmark);
  
  // cleanup
  _body->remove_tunnels();
  // remember to clear `new_eindex_collector'!
  _body->set_new_eindex_collector(0);
  _match.clear();

  // finally, update the adjacency matrix
  _body_m->update(changed_elements);
  //_body_m->init(_body);
}


// match configuration strings
bool
match_config(const String &pat, const String &conf,
	     HashMap<String, String> &defs)
{
  Vector<String> patvec, confvec;
  HashMap<String, String> my_defs;

  // separate into vectors
  cp_argvec(pat, patvec);
  cp_argvec(conf, confvec);

  // not same size -> no match
  if (patvec.size() != confvec.size())
    return false;

  // go over elements and compare; handle $variables
  for (int i = 0; i < patvec.size(); i++) {
    if (patvec[i] == confvec[i])
      continue;
    if (patvec[i].length() <= 1 || patvec[i][0] != '$')
      return false;
    const String &p = patvec[i];
    for (int j = 1; j < p.length(); j++)
      if (!isalnum(p[j]) && p[j] != '_')
	return false;
    if (String *dp = defs.findp(p)) {
      if (*dp != confvec[i])
	return false;
    } else if (String *mp = my_defs.findp(p)) {
      if (*mp != confvec[i])
	return false;
    } else
      my_defs.insert(p, confvec[i]);
  }

  // insert my defs into defs
  for (HashMap<String, String>::Iterator iter = my_defs.first(); iter; iter++)
    defs.insert(iter.key(), iter.value());
  
  return true;
}


#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
#define PATTERNS_OPT		303
#define OUTPUT_OPT		304
#define REVERSE_OPT		305

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "patterns", 'p', PATTERNS_OPT, Clp_ArgString, 0 },
  { "reverse", 'r', REVERSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE] [PATTERNFILE]...\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-xform' replaces patterns of elements with other sets of elements inside\n\
a Click router configuration. Both patterns and configuration are Click-\n\
language files. The transformed router configuration is written to the\n\
standard output.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE] [PATTERNFILE]...\n\
\n\
Options:\n\
  -p, --patterns PATTERNFILE    Read patterns from PATTERNFILE. Can be given\n\
                                more than once.\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -o, --output FILE             Write output to FILE.\n\
  -r, --reverse                 Apply patterns in reverse.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static Vector<RouterT *> patterns;
static Vector<RouterT *> replacements;
static Vector<String> pat_names;
static int patterns_attempted;

void
read_pattern_file(const char *name, ErrorHandler *errh)
{
  patterns_attempted++;
  RouterT *pat_file = read_router_file(name, true, errh);
  if (!pat_file)
    return;

  Vector<ElementClassT *> compounds;
  pat_file->collect_active_types(compounds);
  
  for (int i = 0; i < compounds.size(); i++) {
    String name = compounds[i]->name();
    if (compounds[i]->cast_compound() && name.length() > 12
	&& name.substring(-12) == "_Replacement") {
      ElementClassT *tt = pat_file->try_type(name.substring(0, -12));
      if (tt && tt->cast_compound()) {
	RouterT *rep = compounds[i]->cast_router();
	RouterT *pat = tt->cast_router();
	if (rep && pat) {
	  patterns.push_back(pat);
	  replacements.push_back(rep);
	  pat_names.push_back(name.substring(0, -12));
	}
      }
    }
  }
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  int num_nondash_args = 0;
  const char *router_file = 0;
  const char *output_file = 0;
  bool reverse = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-xform (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 1999-2000 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case PATTERNS_OPT:
      read_pattern_file(clp->arg, errh);
      break;

     case ROUTER_OPT:
      if (router_file) {
	errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case REVERSE_OPT:
      reverse = !clp->negated;
      break;
      
     case Clp_NotOption:
      if (num_nondash_args == 0 && router_file) {
	errh->error("router file specified twice");
	goto bad_option;
      } else if (num_nondash_args == 0)
	router_file = clp->arg;
      else
	read_pattern_file(clp->arg, errh);
      num_nondash_args++;
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

  if (!patterns_attempted)
    errh->warning("no patterns read");

  // manipulate patterns
  if (patterns.size()) {
    // reverse if necessary
    if (reverse) {
      Vector<RouterT *> new_patterns, new_replacements;
      for (int i = patterns.size() - 1; i >= 0; i--) {
	new_patterns.push_back(replacements[i]);
	new_replacements.push_back(patterns[i]);
      }
      patterns.swap(new_patterns);
      replacements.swap(new_replacements);
    }

    // flatten patterns
    for (int i = 0; i < patterns.size(); i++)
      patterns[i]->flatten(errh);
  }
  
  // clear r's flags, so we know the current element complement
  // didn't come from replacements (paranoia)
  for (int i = 0; i < r->nelements(); i++)
    r->element(i)->flags = 0;

  // get adjacency matrices
  Vector<AdjacencyMatrix *> patterns_adj;
  for (int i = 0; i < patterns.size(); i++)
    patterns_adj.push_back(new AdjacencyMatrix(patterns[i]));
  
  bool any = true;
  int nreplace = 0;
  AdjacencyMatrix matrix(r);
  while (any) {
    any = false;
    for (int i = 0; i < patterns.size(); i++) {
      Matcher m(patterns[i], patterns_adj[i], r, &matrix, i + 1, errh);
      if (m.next_match()) {
	m.replace(replacements[i], pat_names[i], String(), errh);
	nreplace++;
	any = true;
	break;
      }
    }
  }

  // write result
  if (nreplace)
    r->remove_dead_elements(0);
  if (write_router_file(r, output_file, errh) < 0)
    exit(1);
  return 0;
}
