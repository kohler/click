/*
 * routert.{cc,hh} -- tool definition of router
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
#include "bitvector.hh"
#include "straccum.hh"
#include <stdio.h>

RouterT::RouterT(RouterT *enclosing)
  : _enclosing_scope(enclosing),
    _element_type_map(-1), _element_name_map(-1)
{
  if (_enclosing_scope)
    _enclosing_scope->use();
  // add space for tunnel and upref types
  _element_type_names.push_back("<tunnel>");
  _element_type_map.insert("<tunnel>", TUNNEL_TYPE);
  _element_classes.push_back(0);
  _element_type_names.push_back("<upref>");
  _element_type_map.insert("<upref>", UPREF_TYPE);
  _element_classes.push_back(0);
}

RouterT::RouterT(const RouterT &o)
  : ElementClassT(),
    _enclosing_scope(o._enclosing_scope),
    _element_type_map(o._element_type_map),
    _element_type_names(o._element_type_names),
    _element_classes(o._element_classes),
    _element_name_map(o._element_name_map),
    _elements(o._elements),
    _hookup_from(o._hookup_from),
    _hookup_to(o._hookup_to),
    _hookup_landmark(o._hookup_landmark)
{
  if (_enclosing_scope)
    _enclosing_scope->use();
  for (int i = 0; i < _element_classes.size(); i++)
    if (_element_classes[i])
      _element_classes[i]->use();
}

RouterT::~RouterT()
{
  _enclosing_scope->unuse();
  for (int i = 0; i < _element_classes.size(); i++)
    if (_element_classes[i])
      _element_classes[i]->unuse();
}


ElementClassT *
RouterT::find_type_class(const String &s) const
{
  const RouterT *r = this;
  while (r) {
    int i = r->_element_type_map[s];
    if (i >= 0) return r->_element_classes[i];
    r = r->_enclosing_scope;
  }
  return 0;
}

int
RouterT::get_type_index(const String &s)
{
  int i = _element_type_map[s];
  if (i >= 0)
    return i;
  else
    return get_type_index(s, find_type_class(s));
}

int
RouterT::get_type_index(const String &s, ElementClassT *eclass)
{
  int i = _element_type_map[s];
  if (i < 0) {
    i = _element_classes.size();
    _element_type_map.insert(s, i);
    _element_type_names.push_back(s);
    _element_classes.push_back(eclass);
    if (eclass) eclass->use();
  }
  return i;
}

int
RouterT::get_anon_type_index(const String &name, ElementClassT *fclass)
{
  int i = _element_classes.size();
  _element_type_names.push_back(name);
  _element_classes.push_back(fclass);
  if (fclass) fclass->use();
  return i;
}

int
RouterT::set_type_index(const String &s, ElementClassT *eclass)
{
  int i = _element_type_map[s];
  if (i < 0 || _element_classes[i] != eclass) {
    i = _element_classes.size();
    _element_type_map.insert(s, i);
    _element_type_names.push_back(s);
    _element_classes.push_back(eclass);
    if (eclass) eclass->use();
  }
  return i;
}

int
RouterT::get_eindex(const String &s, int type_index, const String &config,
		    const String &landmark)
{
  int i = _element_name_map[s];
  if (i < 0 && type_index >= 0) {
    i = _elements.size();
    _elements.push_back(ElementT(s, type_index, config, landmark));
    _element_name_map.insert(s, i);
  }
  return i;
}

int
RouterT::get_anon_eindex(const String &s, int type_index, const String &config,
			 const String &landmark)
{
  int i = _elements.size();
  _elements.push_back(ElementT(s, type_index, config, landmark));
  return i;
}


bool
RouterT::add_connection(const Hookup &hfrom, const Hookup &hto,
			const String &landmark)
{
  int nf = _elements.size();
  if (hfrom.idx >= 0 && hfrom.idx < nf && hto.idx >= 0 && hto.idx < nf) {
    _hookup_from.push_back(hfrom);
    _hookup_to.push_back(hto);
    _hookup_landmark.push_back(landmark);
    return true;
  } else
    return false;
}

void
RouterT::remove_connection(int i)
{
  _hookup_from[i] = _hookup_from.back();
  _hookup_to[i] = _hookup_to.back();
  _hookup_landmark[i] = _hookup_landmark.back();
  _hookup_from.pop_back();
  _hookup_to.pop_back();
  _hookup_landmark.pop_back();
}


bool
RouterT::has_connection(const Hookup &hfrom, const Hookup &hto) const
{
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++)
    if (_hookup_from[i] == hfrom && _hookup_to[i] == hto)
      return true;
  return false;
}

void
RouterT::find_connections_from(const Hookup &h, Vector<Hookup> &v) const
{
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++)
    if (_hookup_from[i] == h)
      v.push_back(_hookup_to[i]);
}

void
RouterT::find_connections_to(const Hookup &h, Vector<Hookup> &v) const
{
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++)
    if (_hookup_to[i] == h)
      v.push_back(_hookup_from[i]);
}

void
RouterT::count_ports(Vector<int> &inputs, Vector<int> &outputs) const
{
  inputs.assign(_elements.size(), 0);
  outputs.assign(_elements.size(), 0);
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    if (hf.port >= outputs[hf.idx])
      outputs[hf.idx] = hf.port + 1;
    if (ht.port >= inputs[ht.idx])
      inputs[ht.idx] = ht.port + 1;
  }
}

bool
RouterT::insert_before(int fidx, const Hookup &h)
{
  Hookup inserter(fidx, 0);
  if (!add_connection(inserter, h))
    return false;
  int nhook = _hookup_from.size() - 1;
  for (int i = 0; i < nhook; i++)
    if (_hookup_to[i] == h)
      _hookup_to[i] = inserter;
  return true;
}

bool
RouterT::insert_after(int fidx, const Hookup &h)
{
  Hookup inserter(fidx, 0);
  if (!add_connection(h, inserter))
    return false;
  int nhook = _hookup_from.size() - 1;
  for (int i = 0; i < nhook; i++)
    if (_hookup_from[i] == h)
      _hookup_from[i] = inserter;
  return true;
}


void
RouterT::add_tunnel(String in, String out, const String &landmark,
		    ErrorHandler *errh)
{
  int in_idx = get_eindex(in, TUNNEL_TYPE, String(), landmark);
  int out_idx = get_eindex(out, TUNNEL_TYPE, String(), landmark);
  if (!errh) errh = ErrorHandler::silent_handler();
  ElementT &fin = _elements[in_idx], &fout = _elements[out_idx];

  if (fin.type != TUNNEL_TYPE)
    errh->lerror(landmark, "element `%s' already exists", in.cc());
  else if (fout.type != TUNNEL_TYPE)
    errh->lerror(landmark, "element `%s' already exists", out.cc());
  else if (fin.tunnel_output >= 0)
    errh->lerror(landmark, "connection tunnel input `%s' already exists", in.cc());
  else if (_elements[out_idx].tunnel_input >= 0)
    errh->lerror(landmark, "connection tunnel output `%s' already exists", out.cc());
  else {
    _elements[in_idx].tunnel_output = out_idx;
    _elements[out_idx].tunnel_input = in_idx;
  }
}


void
RouterT::remove_bad_connections()
{
  // internal use only: invariant -- all connections are good
  int nelements = _elements.size();
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    if (hfrom.idx < 0 || hfrom.idx >= nelements
	|| hto.idx < 0 || hto.idx >= nelements) {
      remove_connection(i);
      i--;
    }
  }
}

void
RouterT::remove_duplicate_connections()
{
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    for (int j = 0; j < i; j++)
      if (_hookup_from[j] == hfrom && _hookup_to[j] == hto) {
	remove_connection(i);
	i--;
	break;
      }
  }
}


void
RouterT::finish_remove_elements(Vector<int> &new_findex, ErrorHandler *errh)
{
  if (!errh) errh = ErrorHandler::silent_handler();
  int nelements = _elements.size();

  // find new findexes
  int j = 0;
  for (int i = 0; i < nelements; i++)
    if (new_findex[i] >= 0)
      new_findex[i] = j++;
  int new_nelements = j;
  
  // change hookup
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    bool bad = false;
    if (hf.idx < 0 || hf.idx >= nelements)
      bad = true;
    else if (new_findex[hf.idx] < 0) {
      errh->lerror(_hookup_landmark[i], "connection from removed element `%s'", ename(hf.idx).cc());
      bad = true;
    } else if (ht.idx < 0 || ht.idx >= nelements)
      bad = true;
    else if (new_findex[ht.idx] < 0) {
      errh->lerror(_hookup_landmark[i], "connection to removed element `%s'", ename(ht.idx).cc());
      bad = true;
    }
    
    if (!bad) {
      hf.idx = new_findex[hf.idx];
      ht.idx = new_findex[ht.idx];
    } else {
      remove_connection(i);
      i--;
    }
  }

  // compress element arrays
  for (int i = 0; i < nelements; i++) {
    j = new_findex[i];
    if (j != i) {
      if (_elements[i].type != UPREF_TYPE)
	_element_name_map.insert(_elements[i].name, j);
      if (j >= 0)
	_elements[j] = _elements[i];
    }
  }

  // massage tunnel pointers
  for (int i = 0; i < new_nelements; i++) {
    ElementT &fac = _elements[i];
    if (fac.tunnel_input >= 0)
      fac.tunnel_input = new_findex[fac.tunnel_input];
    if (fac.tunnel_output >= 0)
      fac.tunnel_output = new_findex[fac.tunnel_output];
  }

  // resize element arrays
  _elements.resize(new_nelements);
}

void
RouterT::remove_blank_elements(ErrorHandler *errh = 0)
{
  int nelements = _elements.size();

  // mark saved findexes
  Vector<int> new_findex(nelements, -1);
  int nftype = _element_classes.size();
  for (int i = 0; i < nelements; i++)
    if (_elements[i].type >= 0 && _elements[i].type < nftype)
      new_findex[i] = 0;

  finish_remove_elements(new_findex, errh);
}

void
RouterT::remove_unconnected_elements()
{
  int nelements = _elements.size();
  Vector<int> new_findex(nelements, -1);

  // mark connected elements
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++) {
    new_findex[_hookup_from[i].idx] = 0;
    new_findex[_hookup_to[i].idx] = 0;
  }

  finish_remove_elements(new_findex, 0);
}


void
RouterT::remove_unused_element_types()
{
  int nelements = _elements.size();
  int nfactype = _element_classes.size();

  // find new ftypeindexes
  Vector<int> new_ftypeidx(nfactype, -1);
  new_ftypeidx[TUNNEL_TYPE] = TUNNEL_TYPE; // save TUNNEL_TYPE
  new_ftypeidx[UPREF_TYPE] = UPREF_TYPE; // save UPREF_TYPE
  for (int i = 0; i < nelements; i++)
    if (_elements[i].type >= 0 && _elements[i].type < nfactype)
      new_ftypeidx[ _elements[i].type ] = 0;
  int j = 0;
  for (int i = 0; i < nfactype; i++)
    if (new_ftypeidx[i] >= 0)
      new_ftypeidx[i] = j++;
  int new_nfactype = j;

  // return if nothing has changed
  assert(new_ftypeidx[TUNNEL_TYPE] == TUNNEL_TYPE);
  assert(new_ftypeidx[UPREF_TYPE] == UPREF_TYPE);
  if (new_nfactype == nfactype)
    return;

  // change elements
  for (int i = 0; i < nelements; i++)
    _elements[i].type = new_ftypeidx[ _elements[i].type ];
  
  // compress element type arrays
  for (int i = 0; i < nfactype; i++) {
    j = new_ftypeidx[i];
    if (j != i) {
      _element_type_map.insert(_element_type_names[i], j);
      if (j >= 0) {
	_element_type_names[j] = _element_type_names[i];
	_element_classes[j] = _element_classes[i];
      } else if (_element_classes[i])
	_element_classes[i]->unuse();
    }
  }

  // resize element type arrays
  _element_type_names.resize(new_nfactype);
  _element_classes.resize(new_nfactype);
}


void
RouterT::expand_tunnel(Vector<Hookup> *pp_expansions,
		       bool is_input, int which,
		       Vector<Hookup> &results) const
{
  Vector<Hookup> &ppx = pp_expansions[which];
  
  // quit if circular
  if (ppx.size() == 1 && ppx[0].idx == -2)
    return;

  if (ppx.size() == 1 && ppx[0].idx == -1) {
    ppx[0].idx = -2;
    Vector<Hookup> expansion;
    Vector<Hookup> cur_expansion;
    // find connections from the corresponding tunnel output
    if (is_input)
      find_connections_from(Hookup(-1, which), cur_expansion);
    else			// or to corresponding tunnel input
      find_connections_to(Hookup(-1, which), cur_expansion);
    // expand them
    for (int i = 0; i < cur_expansion.size(); i++)
      if (cur_expansion[i].idx < 0)
	expand_tunnel(pp_expansions, is_input, cur_expansion[i].port,
		      expansion);
      else
	expansion.push_back(cur_expansion[i]);
    // save results
    ppx.swap(expansion);
  }

  // append results
  for (int i = 0; i < ppx.size(); i++)
    results.push_back(ppx[i]);
}

void
RouterT::remove_tunnels(ErrorHandler *errh)
{
  // find tunnel connections, mark connections with fidx -1
  Vector<Hookup> tunnel_in, tunnel_out;
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    int pp_in = _elements[hfrom.idx].tunnel_input;
    int pp_out = _elements[hto.idx].tunnel_output;
    if (pp_in >= 0) {
      int j = hfrom.index_in(tunnel_out);
      if (j < 0) {
	tunnel_in.push_back(Hookup(pp_in, hfrom.port));
	tunnel_out.push_back(hfrom);
	j = tunnel_in.size() - 1;
      }
      hfrom.idx = -1;
      hfrom.port = j;
    }
    if (pp_out >= 0) {
      int j = hto.index_in(tunnel_in);
      if (j < 0) {
	tunnel_in.push_back(hto);
	tunnel_out.push_back(Hookup(pp_out, hto.port));
	j = tunnel_in.size() - 1;
      }
      hto.idx = -1;
      hto.port = j;
    }
  }

  // expand tunnels
  int npp = tunnel_in.size();
  Vector<Hookup> *ppin_expansions = new Vector<Hookup>[npp];
  Vector<Hookup> *ppout_expansions = new Vector<Hookup>[npp];
  for (int i = 0; i < npp; i++) {
    // initialize to placeholders
    ppin_expansions[i].push_back(Hookup(-1, 0));
    ppout_expansions[i].push_back(Hookup(-1, 0));
  }
  
  // get rid of connections to tunnels
  int nelements = _elements.size();
  int old_nhook = _hookup_from.size();
  for (int i = 0; i < old_nhook; i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    const String &landmark = _hookup_landmark[i];
    
    // skip if uninteresting
    if (hfrom.idx >= 0)
      continue;
    
    // find first-level connections
    Vector<Hookup> new_from, new_to;
    if (hfrom.idx < 0)
      expand_tunnel(ppout_expansions, false, hfrom.port, new_from);
    else
      new_from.push_back(hfrom);
    if (hto.idx < 0)
      expand_tunnel(ppin_expansions, true, hto.port, new_to);
    else
      new_to.push_back(hto);
    
    // add cross product
    for (int j = 0; j < new_from.size(); j++)
      for (int k = 0; k < new_to.size(); k++)
	add_connection(new_from[j], new_to[k], landmark);
  }

  // kill elements with tunnel type
  // but don't kill floating tunnels (like input & output)
  for (int i = 0; i < nelements; i++)
    if (_elements[i].type == TUNNEL_TYPE
	&& (_elements[i].tunnel_output >= 0
	    || _elements[i].tunnel_input >= 0))
      _elements[i].type = -1;

  // actually remove tunnel connections and elements
  remove_bad_connections();
  remove_blank_elements(errh);
  remove_duplicate_connections();
}


static int
resolve_upref(const String &upref, String prefix, RouterT *r)
{
  while (prefix) {
    int pos = prefix.find_right('/', prefix.length() - 2);
    prefix = (pos >= 0 ? prefix.substring(0, pos + 1) : String());
    
    String try_name = prefix + upref;
    int en = r->eindex(try_name);
    if (en >= 0) return en;
  }
  return -1;
}

void
RouterT::expand_compound(ElementT &compound, RouterT *r, ErrorHandler *errh)
{
  // complain about configuration string
  if (compound.configuration && errh)
    errh->lerror(compound.landmark, "compound element `%s' given configuration string", compound.name.cc());

  // create prefix
  String prefix;
  assert(compound.name);
  if (compound.name[compound.name.length() - 1] == '/')
    prefix = compound.name;
  else
    prefix = compound.name + "/";
  
  // create input/output tunnels
  compound.type = TUNNEL_TYPE;
  r->add_tunnel(compound.name, prefix + "input", compound.landmark, errh);
  r->add_tunnel(prefix + "output", compound.name, compound.landmark, errh);

  int nelements = _elements.size();
  Vector<int> new_fidx(nelements, -1);
  
  // add tunnel pairs and resolve uprefs
  for (int i = 0; i < nelements; i++) {
    ElementT &e = _elements[i];
    if (e.type == TUNNEL_TYPE && e.tunnel_output >= 0
	&& e.tunnel_output < nelements)
      r->add_tunnel(prefix + e.name,
		    prefix + _elements[e.tunnel_output].name,
		    e.landmark, errh);
    else if (e.type == UPREF_TYPE)
      new_fidx[i] = resolve_upref(e.name, prefix, r);
  }
  
  // add component elements
  for (int i = 0; i < _elements.size(); i++) {
    ElementT &e = _elements[i];
    if (new_fidx[i] >= 0) continue; // skip elements we've already resolved
    
    // get element type index. add new "anonymous" element type if needed
    int ftypi = r->get_type_index(_element_type_names[e.type],
				  _element_classes[e.type]);
    if (_element_classes[e.type] != r->_element_classes[ftypi]
	&& r->_element_classes[ftypi])
      ftypi = r->get_anon_type_index(_element_type_names[e.type],
				     _element_classes[e.type]);
    
    // add element
    if (e.type == UPREF_TYPE)
      // add unresolved uprefs, but w/o prefix
      new_fidx[i] = r->get_anon_eindex
	(e.name, UPREF_TYPE, e.configuration, e.landmark);
    else
      new_fidx[i] = r->get_eindex
	(prefix + e.name, ftypi, e.configuration, e.landmark);
  }
  
  // add hookup
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    r->add_connection(Hookup(new_fidx[hfrom.idx], hfrom.port),
		      Hookup(new_fidx[hto.idx], hto.port),
		      _hookup_landmark[i]);
  }
}

void
RouterT::remove_compound_elements(ErrorHandler *errh)
{
  for (int i = 0; i < _elements.size(); i++) {
    int typ = _elements[i].type;
    if (typ < 0 || typ >= _element_classes.size() || !_element_classes[typ])
      continue;
    _element_classes[typ]->expand_compound(_elements[i], this, errh);
  }

  remove_blank_elements(errh);
  remove_unused_element_types();
}

void
RouterT::remove_unresolved_uprefs(ErrorHandler *errh)
{
  if (!errh) errh = ErrorHandler::silent_handler();
  
  int nelements = _elements.size();
  Vector<int> new_findex(nelements, 0);
  bool any = false;
  
  // find uprefs
  for (int i = 0; i < nelements; i++) {
    ElementT &e = _elements[i];
    if (e.type == UPREF_TYPE) {
      errh->lerror(e.landmark, "unresolved upref `%s'", e.name.cc());
      new_findex[i] = -1;
      any = true;
    }
  }

  if (any) finish_remove_elements(new_findex, 0);
}

void
RouterT::flatten(ErrorHandler *errh)
{
  remove_compound_elements(errh);
  remove_tunnels(errh);
  remove_unresolved_uprefs(errh);
}


// MATCHES

bool
RouterT::next_element_match(RouterT *pat, Vector<int> &match) const
{
  int pnf = pat->_elements.size();
  int nf = _elements.size();
  int nft = _element_classes.size();
  
  int match_idx;
  int direction;
  if (match.size() == 0) {
    match.assign(pnf, -1);
    int pnft = pat->_element_classes.size();
    for (int i = 0; i < pnf; i++) {
      int t = pat->_elements[i].type;
      if (t == TUNNEL_TYPE)
	match[i] = -2;		// don't match tunnels
      else if (t < 0 || t >= pnft)
	match[i] = -3;		// don't match bad types
    }
    match_idx = 0;
    direction = 1;
  } else {
    match_idx = pnf - 1;
    direction = -1;
  }

  while (match_idx >= 0 && match_idx < pnf) {
    if (match[match_idx] < -1) {
      match_idx += direction;
      continue;
    }
    int rover = match[match_idx] + 1;
    const String &want_type = pat->_element_type_names[ pat->_elements[match_idx].type ];
    while (rover < nf) {
      int t = _elements[rover].type;
      if (t >= 0 && t < nft && _element_type_names[t] == want_type) {
	for (int j = 0; j < match_idx; j++)
	  if (match[j] == rover)
	    goto not_match;
	break;
      }
     not_match: rover++;
    }
    if (rover < nf) {
      match[match_idx] = rover;
      match_idx++;
      direction = 1;
    } else {
      match[match_idx] = -1;
      match_idx--;
      direction = -1;
    }
  }

  return (match_idx >= 0 ? true : false);
}

bool
RouterT::next_connection_match(RouterT *pat, Vector<int> &match) const
{
  int pnh = pat->_hookup_from.size();
  while (1) {
    if (!next_element_match(pat, match))
      return false;

    // check connections
    for (int i = 0; i < pnh; i++) {
      const Hookup &phf = pat->_hookup_from[i], &pht = pat->_hookup_to[i];
      if (match[phf.idx] >= 0 && match[pht.idx] >= 0)
	if (!has_connection(Hookup(match[phf.idx], phf.port),
			    Hookup(match[pht.idx], pht.port)))
	  goto not_match;
    }
    return true;
    
   not_match: /* try again */;
  }
}

static bool
nonexclusive_output(const Hookup &hout, RouterT *pat, Vector<int> &match)
{
  const Vector<Hookup> &hfrom = pat->hookup_from();
  const Vector<Hookup> &hto = pat->hookup_to();
  for (int i = 0; i < hfrom.size(); i++)
    if (hfrom[i] == hout && match[hto[i].idx] == -2)
      return true;
  return false;
}

static bool
nonexclusive_input(const Hookup &hout, RouterT *pat, Vector<int> &match)
{
  const Vector<Hookup> &hfrom = pat->hookup_from();
  const Vector<Hookup> &hto = pat->hookup_to();
  for (int i = 0; i < hfrom.size(); i++)
    if (hto[i] == hout && match[hfrom[i].idx] == -2)
      return true;
  return false;
}

bool
RouterT::next_exclusive_connection_match(RouterT *pat, Vector<int> &match) const
{
  int pnf = pat->_elements.size();
  int nf = _elements.size();
  int nh = _hookup_from.size();
  while (1) {
    if (!next_connection_match(pat, match))
      return false;

    // create backmapping
    Vector<int> back_match(nf, -1);
    for (int i = 0; i < pnf; i++)
      if (match[i] >= 0)
	back_match[match[i]] = i;
    
    // check connections
    for (int i = 0; i < nh; i++) {
      const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
      int pf = back_match[hf.idx], pt = back_match[ht.idx];
      if (pf >= 0 && pt >= 0) {
	if (!pat->has_connection(Hookup(pf, hf.port), Hookup(pt, ht.port)))
	  goto not_match;
      } else if (pf >= 0 && pt < 0) {
	if (!nonexclusive_output(Hookup(pf, hf.port), pat, match))
	  goto not_match;
      } else if (pf < 0 && pt >= 0) {
	if (!nonexclusive_input(Hookup(pt, ht.port), pat, match))
	  goto not_match;
      } /* otherwise, connection between two non-match elements; don't care */
    }
    return true;
    
   not_match: /* try again */;
  }
}


// PRINTING

void
RouterT::compound_declaration_string(StringAccum &sa, const String &name,
				     const String &indent)
{
  sa << indent << "elementclass " << name << " {\n";
  configuration_string(sa, indent + "  ");
  sa << indent << "}\n";
}

String
RouterT::ename_upref(int idx) const
{
  if (idx >= 0 && idx < _elements.size()) {
    if (_elements[idx].type == UPREF_TYPE)
      return "^" + _elements[idx].name;
    else
      return _elements[idx].name;
  } else
    return String("/*BAD_") + String(idx) + String("*/");
}

void
RouterT::configuration_string(StringAccum &sa, const String &indent) const
{
  int nelements = _elements.size();
  int nelemtype = _element_classes.size();

  // print element classes
  int old_sa_len = sa.length();
  for (int i = 0; i < nelemtype; i++)
    if (_element_classes[i])
      _element_classes[i]->compound_declaration_string
	(sa, _element_type_names[i], indent);
  if (sa.length() != old_sa_len)
    sa << "\n";
  
  // print tunnel pairs
  old_sa_len = sa.length();
  for (int i = 0; i < nelements; i++)
    if (_elements[i].type == TUNNEL_TYPE
	&& _elements[i].tunnel_output >= 0
	&& _elements[i].tunnel_output < nelements) {
      sa << indent << "connectiontunnel " << _elements[i].name << " -> "
	 << _elements[ _elements[i].tunnel_output ].name << ";\n";
    }
  if (sa.length() != old_sa_len)
    sa << "\n";
  
  // print element types
  old_sa_len = sa.length();
  for (int i = 0; i < nelements; i++) {
    const ElementT &e = _elements[i];
    if (e.type == TUNNEL_TYPE || e.type == UPREF_TYPE)
      continue; // skip tunnels and uprefs
    sa << indent << e.name << " :: ";
    if (e.type >= 0 && e.type < nelemtype)
      sa << _element_type_names[e.type];
    else
      sa << "/*BAD_TYPE_" << e.type << "*/";
    if (e.configuration)
      sa << "(" << e.configuration << ")";
    sa << ";\n";
  }
  if (sa.length() != old_sa_len)
    sa << "\n";

  // prepare hookup chains
  int nhookup = _hookup_from.size();
  Vector<int> next(nhookup, -1);
  Bitvector startchain(nhookup, true);
  for (int c = 0; c < nhookup; c++) {
    const Hookup &ht = _hookup_to[c];
    if (ht.port != 0) continue;
    int result = -1;
    for (int d = 0; d < nhookup; d++)
      if (d != c && _hookup_from[d] == ht) {
	result = d;
	if (_hookup_to[d].port == 0)
	  break;
      }
    if (result >= 0) {
      next[c] = result;
      startchain[result] = false;
    }
  }
  
  // print hookup
  Bitvector used(nhookup, false);
  bool done = false;
  while (!done) {
    // print chains
    for (int c = 0; c < nhookup; c++) {
      if (used[c] || !startchain[c]) continue;
      
      const Hookup &hf = _hookup_from[c];
      sa << indent << ename_upref(hf.idx);
      if (hf.port)
	sa << " [" << hf.port << "]";
      
      int d = c;
      while (d >= 0 && !used[d]) {
	if (d == c) sa << " -> ";
	else sa << "\n" << indent << "    -> ";
	const Hookup &ht = _hookup_to[d];
	if (ht.port)
	  sa << "[" << ht.port << "] ";
	sa << ename_upref(ht.idx);
	used[d] = true;
	d = next[d];
      }
      
      sa << ";\n";
    }

    // add new chains to include cycles
    done = true;
    for (int c = 0; c < nhookup && done; c++)
      if (!used[c])
	startchain[c] = true, done = false;
  }
}

String
RouterT::configuration_string() const
{
  StringAccum sa;
  configuration_string(sa);
  return sa.take_string();
}
