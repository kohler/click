/*
 * click-xform.cc -- pattern-based optimizer for Click configurations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2007 Regents of the University of California
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

bool match_config(const String &, const String &, HashTable<String, String> &);
// TODO: allow some special pports to be unconnected

class Matcher { public:

  Matcher(RouterT *, AdjacencyMatrix *, RouterT *, AdjacencyMatrix *, int, ErrorHandler *);
  ~Matcher();

  bool check_into(const PortT &, const PortT &);
  bool check_out_of(const PortT &, const PortT &);

  bool check_match();
  bool next_match();

  void replace_config(ElementT *) const;
  void replace(RouterT *, const String &, const LandmarkT &, ErrorHandler *);

 private:

  RouterT *_pat;
  AdjacencyMatrix *_pat_m;
  RouterT *_body;
  AdjacencyMatrix *_body_m;
  int _patid;

  ElementT *_pat_input;
  ElementT *_pat_output;

  Vector<ElementT *> _match;
  Vector<ElementT *> _back_match;
  HashTable<String, String> _defs;

  Vector<PortT> _to_pp_from;
  Vector<PortT> _to_pp_to;
  Vector<PortT> _from_pp_from;
  Vector<PortT> _from_pp_to;

};


Matcher::Matcher(RouterT *pat, AdjacencyMatrix *pat_m,
		 RouterT *body, AdjacencyMatrix *body_m,
		 int patid, ErrorHandler *errh)
  : _pat(pat), _pat_m(pat_m), _body(body), _body_m(body_m), _patid(patid),
    _pat_input(0), _pat_output(0)
{
  // check tunnel situation
  for (RouterT::iterator x = _pat->begin_elements(); x; x++) {
    if (!x->tunnel())
      /* nada */;
    else if (x->tunnel_connected())
      errh->lerror(x->landmark(), "pattern has active connection tunnels");
    else if (x->name() == "input" && !_pat_input)
      _pat_input = x.get();
    else if (x->name() == "output" && !_pat_output)
      _pat_output = x.get();
    else
      errh->lerror(x->landmark(), "connection tunnel with funny name '%s'", x->name_c_str());
  }
}

Matcher::~Matcher()
{
}

bool
Matcher::check_into(const PortT &houtside, const PortT &hinside)
{
  PortT phinside(_back_match[hinside.eindex()], hinside.port);
  PortT success;
  // now look for matches
  for (RouterT::conn_iterator it = _pat->find_connections_to(phinside);
       it != _pat->end_connections(); ++it)
    if (it->from_element() == _pat_input
	&& (success.dead() || it->from() < success)) {
      Vector<PortT> pfrom_phf, from_houtside;
      // check that it's valid: all connections from tunnels are covered
      // in body
      _pat->find_connections_from(it->from(), pfrom_phf);
      _body->find_connections_from(houtside, from_houtside);
      for (int j = 0; j < pfrom_phf.size(); j++) {
	PortT want(_match[pfrom_phf[j].eindex()], pfrom_phf[j].port);
	if (want.index_in(from_houtside) < 0)
	  goto no_match;
      }
      // success: save it
      success = it->from();
     no_match: ;
    }
  // if succeeded, save it
  if (success.live()) {
    _to_pp_from.push_back(houtside);
    _to_pp_to.push_back(success);
    return true;
  } else
    return false;
}

bool
Matcher::check_out_of(const PortT &hinside, const PortT &houtside)
{
  PortT phinside(_back_match[hinside.eindex()], hinside.port);
  PortT success;
  // now look for matches
  for (RouterT::conn_iterator it = _pat->find_connections_from(phinside);
       it != _pat->end_connections(); ++it)
    if (it->to_element() == _pat_output
	&& (success.dead() || it->to() < success)) {
      Vector<PortT> pto_pht, to_houtside;
      // check that it's valid: all connections to tunnels are covered
      // in body
      _pat->find_connections_to(it->to(), pto_pht);
      _body->find_connections_to(houtside, to_houtside);
      for (int j = 0; j < pto_pht.size(); j++) {
	PortT want(_match[pto_pht[j].eindex()], pto_pht[j].port);
	if (want.index_in(to_houtside) < 0)
	  goto no_match;
      }
      // success: save it
      success = it->to();
     no_match: ;
    }
  // if succeeded, save it
  if (success.live()) {
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
    if (_match[i]) {
      if (!match_config(_pat->element(i)->configuration(), _match[i]->configuration(), _defs))
	return false;
    }

  int bnf = _body->nelements();
  _back_match.assign(bnf, 0);
  bool all_previous_match = true;
  for (int i = 0; i < _match.size(); i++)
    if (ElementT *m = _match[i]) {
      _back_match[m->eindex()] = _pat->element(i);
      if (m->flags != _patid)	// doesn't come from replacement of same pat
	all_previous_match = false;
    }

  // don't allow match if it consists entirely of elements previously replaced
  // by this pattern
  if (all_previous_match)
    return false;

  // find the pattern ports any cross-pattern jumps correspond to
  //fprintf(stderr, "XPJ\n");
  for (RouterT::conn_iterator it = _body->begin_connections();
       it != _body->end_connections(); ++it) {
    const PortT &hf = it->from(), &ht = it->to();
    ElementT *pf = _back_match[hf.eindex()], *pt = _back_match[ht.eindex()];
    if (pf && pt) {
      if (!_pat->has_connection(PortT(pf, hf.port), PortT(pt, ht.port)))
	return false;
    } else if (!pf && pt) {
      if (!check_into(hf, ht))
	return false;
    } else if (pf && !pt) {
      if (!check_out_of(hf, ht))
	return false;
    }
  }

  // check for unconnected tunnels in the pattern
  //fprintf(stderr, "UNC\n");
  for (RouterT::conn_iterator it = _pat->begin_connections();
       it != _body->end_connections(); ++it) {
    if (it->from_element() == _pat_input
	&& it->from().index_in(_to_pp_to) < 0)
      return false;
    if (it->to_element() == _pat_output
	&& it->to().index_in(_from_pp_from) < 0)
      return false;
  }

  //fprintf(stderr, "  YES\n");
  return true;
}

bool
Matcher::next_match()
{
  while (_pat_m->next_subgraph_isomorphism(_body_m, _match)) {
    //fprintf(stderr, "NEXT\n");
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
  static HashTable<String, int> *last_uniqueifier;
  if (!last_uniqueifier)
    last_uniqueifier = new HashTable<String, int>(1);
  int count = last_uniqueifier->get(base_prefix);

  while (1) {
    String prefix = base_prefix + "@" + String(count);
    count++;

    // look for things starting with that name
    int plen = prefix.length();
    for (RouterT::iterator x = r->begin_elements(); x; x++) {
      const String &n = x->name();
      if (n.length() > plen + 1 && n.substring(0, plen) == prefix
	  && n[plen] == '/')
	goto failed;
    }

    last_uniqueifier->set(base_prefix, count);
    return prefix;

   failed: ;
  }
}

void
Matcher::replace_config(ElementT *e) const
{
  Vector<String> confvec;
  cp_argvec(e->configuration(), confvec);

  bool changed = false;
  for (int i = 0; i < confvec.size(); i++) {
    if (confvec[i].length() <= 1 || confvec[i][0] != '$')
      continue;
    if (HashTable<String, String>::const_iterator it = _defs.find(confvec[i])) {
      confvec[i] = it.value();
      changed = true;
    }
  }

  if (changed)
    e->set_configuration(cp_unargvec(confvec));
}

void
Matcher::replace(RouterT *replacement, const String &try_prefix,
		 const LandmarkT &landmark, ErrorHandler *errh)
{
  //fprintf(stderr, "replace...\n");
  String prefix = uniqueify_prefix(try_prefix, _body);

  // free old elements
  Vector<int> changed_elements;
  Vector<String> old_names;
  for (int i = 0; i < _match.size(); i++)
    if (_match[i]) {
      changed_elements.push_back(_match[i]->eindex());
      old_names.push_back(_match[i]->name());
      _body->free_element(_match[i]);
    } else
      old_names.push_back(String());

  // add replacement
  // collect new element indices in 'changed_elements'
  _body->set_new_eindex_collector(&changed_elements);

  // make element named 'prefix'
  ElementT *new_e = _body->get_element(prefix, ElementClassT::tunnel_type(), String(), landmark);

  // expand 'replacement' into '_body'; need crap compound element
  Vector<String> crap_args;
  VariableEnvironment crap_ve(0);
  replacement->complex_expand_element(new_e, crap_args, _body, String(), crap_ve, errh);

  // mark replacement
  for (int i = 0; i < changed_elements.size(); i++) {
    ElementT *e = _body->element(changed_elements[i]);
    if (e->dead())
      continue;
    e->flags = _patid;
    replace_config(e);
  }

  // save old element name if matched element and some replacement element
  // have the same name
  for (int i = 0; i < _match.size(); i++)
    if (_match[i]) {
      String n = _pat->ename(i);
      int new_index = _body->eindex(prefix + "/" + n);
      if (new_index >= 0)
	_body->change_ename(new_index, old_names[i]);
    }

  // find input and output, add connections to tunnels
  ElementT *new_pp = _body->element(prefix);
  for (int i = 0; i < _to_pp_from.size(); i++)
    _body->add_connection(_to_pp_from[i], PortT(new_pp, _to_pp_to[i].port),
			  landmark);
  for (int i = 0; i < _from_pp_from.size(); i++)
    _body->add_connection(PortT(new_pp, _from_pp_from[i].port),
			  _from_pp_to[i], landmark);

  // cleanup
  _body->remove_tunnels();
  // remember to clear 'new_eindex_collector'!
  _body->set_new_eindex_collector(0);
  _match.clear();

  // finally, update the adjacency matrix
  _body_m->update(changed_elements);
  //_body_m->init(_body);
}


// match configuration strings
bool
match_config(const String &pat, const String &conf,
	     HashTable<String, String> &defs)
{
  Vector<String> patvec, confvec;
  HashTable<String, String> my_defs;

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
      if (!isalnum((unsigned char) p[j]) && p[j] != '_')
	return false;
    if (HashTable<String, String>::iterator it = defs.find(p)) {
      if (it.value() != confvec[i])
	return false;
    } else if (HashTable<String, String>::iterator it = my_defs.find(p)) {
      if (it.value() != confvec[i])
	return false;
    } else
      my_defs.set(p, confvec[i]);
  }

  // insert my defs into defs
  for (HashTable<String, String>::iterator iter = my_defs.begin(); iter.live(); iter++)
    defs.set(iter.key(), iter.value());

  return true;
}


#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define PATTERNS_OPT		306
#define REVERSE_OPT		307

static const Clp_Option options[] = {
  { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
  { "patterns", 'p', PATTERNS_OPT, Clp_ValString, 0 },
  { "reverse", 'r', REVERSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE] [PATTERNFILE]...\n\
Try '%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
'Click-xform' replaces patterns of elements with other sets of elements inside\n\
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
  -e, --expression EXPR         Use EXPR as router configuration.\n\
  -o, --output FILE             Write output to FILE.\n\
  -r, --reverse                 Apply patterns in reverse.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
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
  pat_file->collect_locally_declared_types(compounds);

  for (int i = 0; i < compounds.size(); i++) {
    String name = compounds[i]->name();
    if (compounds[i]->cast_router() && name.length() > 12
	&& name.substring(-12) == "_Replacement") {
      ElementClassT *tt = pat_file->locally_declared_type(name.substring(0, -12));
      if (tt && tt->cast_router()) {
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
  click_static_initialize();
  CLICK_DEFAULT_PROVIDES;
  ErrorHandler *errh = ErrorHandler::default_handler();

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  int num_nondash_args = 0;
  const char *router_file = 0;
  bool file_is_expr = false;
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
      read_pattern_file(clp->vstr, errh);
      break;

     case ROUTER_OPT:
     case EXPRESSION_OPT:
      if (router_file) {
	errh->error("router configuration specified twice");
	goto bad_option;
      }
      router_file = clp->vstr;
      file_is_expr = (opt == EXPRESSION_OPT);
      break;

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->vstr;
      break;

     case REVERSE_OPT:
      reverse = !clp->negated;
      break;

     case Clp_NotOption:
      if (click_maybe_define(clp->vstr, errh))
	  break;
      if (num_nondash_args == 0 && router_file) {
	errh->error("router configuration specified twice");
	goto bad_option;
      } else if (num_nondash_args == 0)
	router_file = clp->vstr;
      else
	read_pattern_file(clp->vstr, errh);
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
  RouterT *r = read_router(router_file, file_is_expr, errh);
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
	m.replace(replacements[i], pat_names[i], LandmarkT(), errh);
	nreplace++;
	any = true;
	break;
      }
    }
  }

  // write result
  if (nreplace)
    r->remove_dead_elements();
  if (write_router_file(r, output_file, errh) < 0)
    exit(1);
  return 0;
}
