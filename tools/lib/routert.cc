/*
 * routert.{cc,hh} -- tool definition of router
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
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
#include "confparse.hh"
#include "straccum.hh"
#include <stdio.h>

RouterT::RouterT(RouterT *enclosing)
  : _enclosing_scope(enclosing),
    _element_type_map(-1), _element_name_map(-1),
    _require_map(-1), _archive_map(-1)
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
    _hookup_landmark(o._hookup_landmark),
    _require_map(o._require_map),
    _archive_map(o._archive_map),
    _archive(o._archive)
{
  if (_enclosing_scope)
    _enclosing_scope->use();
  for (int i = 0; i < _element_classes.size(); i++)
    if (_element_classes[i])
      _element_classes[i]->use();
}

RouterT::~RouterT()
{
  if (_enclosing_scope)
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
  if (i < 0) {
    assert(type_index >= 0 && type_index < ntypes());
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
  assert(type_index >= 0 && type_index < ntypes());
  int i = _elements.size();
  _elements.push_back(ElementT(s, type_index, config, landmark));
  return i;
}

int
RouterT::get_anon_eindex(int type_index, const String &config,
			 const String &landmark)
{
  String name = _element_type_names[type_index] + "@" + String(_elements.size() + 1);
  return get_anon_eindex(name, type_index, config, landmark);
}

void
RouterT::get_types_from(const RouterT *r)
{
  for (int i = 0; i < r->ntypes(); i++)
    get_type_index(r->type_name(i), r->type_class(i));
}

int
RouterT::unify_type_indexes(const RouterT *r)
{
  Vector<int> new_tidx;
  for (int i = 0; i < ntypes(); i++) {
    int t = r->type_index( type_name(i) );
    if (t < 0)
      return -1;
    new_tidx.push_back(t);
  }

  // trash old element classes, make new element classes
  for (int i = 0; i < _element_classes.size(); i++)
    if (_element_classes[i])
      _element_classes[i]->unuse();

  _element_type_map = r->_element_type_map;
  _element_type_names = r->_element_type_names;
  _element_classes = r->_element_classes;
  for (int i = 0; i < _element_classes.size(); i++)
    if (_element_classes[i])
      _element_classes[i]->use();

  // fix tindexes
  for (int i = 0; i < nelements(); i++)
    element(i).type = new_tidx[ element(i).type ];
  
  return 0;
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
RouterT::add_requirement(const String &s)
{
  _require_map.insert(s, 0);
}


void
RouterT::add_archive(const ArchiveElement &ae)
{
  int i = _archive_map[ae.name];
  if (i >= 0)
    _archive[i] = ae;
  else {
    _archive_map.insert(ae.name, _archive.size());
    _archive.push_back(ae);
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
  // 5.Dec.1999 - This function dominated the running time of click-xform. Use
  // an algorithm faster on the common case (few connections per element).

  int nh = _hookup_from.size();
  int nelem = _elements.size();
  Vector<int> index(nelem, -1);
  Vector<int> next(nh, -1);
  Vector<int> removers;
  
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    int prev = -1;
    int trav = index[hfrom.idx];
    while (trav >= 0) {
      if (_hookup_from[trav].port == hfrom.port && _hookup_to[trav] == hto) {
	removers.push_back(i);
	goto removed;
      }
      prev = trav;
      trav = next[trav];
    }
    if (prev == -1)
      index[hfrom.idx] = i;
    else
      next[prev] = i;
   removed: ;
  }

  for (int i = removers.size() - 1; i >= 0; i--)
    remove_connection(removers[i]);
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
    if (hf.idx < 0 || hf.idx >= nelements || ht.idx < 0 || ht.idx >= nelements)
      bad = true;
    else if (new_findex[hf.idx] < 0) {
      errh->lerror(_hookup_landmark[i], "connection from removed element `%s'", ename(hf.idx).cc());
      bad = true;
    } else if (new_findex[ht.idx] < 0) {
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
RouterT::finish_remove_element_types(Vector<int> &new_tindex)
{
  int ntype = _element_classes.size();
  int nelements = _elements.size();

  // find new ftypeindexes
  // save TUNNEL_TYPE and UPREF_TYPE
  new_tindex[TUNNEL_TYPE] = new_tindex[UPREF_TYPE] = 0;
  int j = 0;
  for (int i = 0; i < ntype; i++)
    if (new_tindex[i] >= 0)
      new_tindex[i] = j++;
  int new_ntype = j;

  // return if nothing has changed
  assert(new_tindex[TUNNEL_TYPE] == TUNNEL_TYPE);
  assert(new_tindex[UPREF_TYPE] == UPREF_TYPE);
  if (new_ntype == ntype)
    return;

  // change elements
  for (int i = 0; i < nelements; i++)
    _elements[i].type = new_tindex[ _elements[i].type ];
  
  // compress element type arrays
  _element_type_map.clear();
  for (int i = 0; i < ntype; i++) {
    j = new_tindex[i];
    if (j >= 0) {
      _element_type_map.insert(_element_type_names[i], j);
      _element_type_names[j] = _element_type_names[i];
      _element_classes[j] = _element_classes[i];
    } else if (_element_classes[i])
      _element_classes[i]->unuse();
  }

  // resize element type arrays
  _element_type_names.resize(new_ntype);
  _element_classes.resize(new_ntype);
}

void
RouterT::remove_unused_element_types()
{
  Vector<int> new_tindex(_element_classes.size(), -1);
  int nelem = _elements.size();
  for (int i = 0; i < nelem; i++)
    new_tindex[ _elements[i].type ] = 0;
  finish_remove_element_types(new_tindex);
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


RouterScope::RouterScope(const RouterScope &o, const String &suffix)
  : _prefix(o._prefix + suffix), _formals(o._formals), _values(o._values)
{
}

void
RouterScope::combine(const Vector<String> &formals, const Vector<String> &values)
{
  for (int i = 0; i < formals.size(); i++) {
    for (int j = 0; j < _formals.size(); j++)
      if (_formals[j] == formals[i]) {
	_values[j] = values[i];
	goto done;
      }
    _formals.push_back(formals[i]);
    _values.push_back(values[i]);
   done: ;
  }
}

String
RouterScope::interpolate(const String &config) const
{
  if (_formals.size() == 0)
    return config;
  
  const char *data = config.data();
  int config_pos = 0;
  int pos = 0;
  int len = config.length();
  String output;
  
  for (; pos < len; pos++)
    if (data[pos] == '\\' && pos < len - 1)
      pos++;
    else if (data[pos] == '/' && pos < len - 1) {
      if (data[pos+1] == '/') {
	for (pos += 2; pos < len && data[pos] != '\n' && data[pos] != '\r'; )
	  pos++;
      } else if (data[pos+1] == '*') {
	for (pos += 2; pos < len; pos++)
	  if (data[pos] == '*' && pos < len - 1 && data[pos+1] == '/') {
	    pos++;
	    break;
	  }
      }
    } else if (data[pos] == '$') {
      unsigned word_pos = pos;
      for (pos++; isalnum(data[pos]) || data[pos] == '_'; pos++)
	/* nada */;
      String name = config.substring(word_pos, pos - word_pos);
      for (int variable = 0; variable < _formals.size(); variable++)
	if (name == _formals[variable]) {
	  output += config.substring(config_pos, word_pos - config_pos);
	  output += _values[variable];
	  config_pos = pos;
	}
      pos--;
    }

  if (!output)
    return config;
  else
    return output + config.substring(config_pos, pos - config_pos);
}

int
RouterT::expand_into(RouterT *fromr, int which, RouterT *tor,
		     const RouterScope &scope, ErrorHandler *errh)
{
  assert(fromr != this && tor != this);
  ElementT &compound = fromr->element(which);
  
  // parse configuration string
  Vector<String> args;
  int nargs = _formals.size();
  cp_argvec_unsubst(scope.interpolate(compound.configuration), args);
  if (args.size() != nargs) {
    const char *whoops = (args.size() < nargs ? "few" : "many");
    String signature;
    for (int i = 0; i < nargs; i++) {
      if (i) signature += ", ";
      signature += _formals[i];
    }
    if (errh)
      errh->lerror(compound.landmark,
		   "too %s arguments to compound element `%s(%s)'", whoops,
		   compound.name.cc() /* XXX should be class_name */, signature.cc());
    for (int i = args.size(); i < nargs; i++)
      args.push_back("");
  }

  // create prefix
  String suffix;
  assert(compound.name);
  if (compound.name[compound.name.length() - 1] == '/')
    suffix = compound.name;
  else
    suffix = compound.name + "/";
  
  RouterScope new_scope(scope, suffix);
  String prefix = scope.prefix();
  String new_prefix = new_scope.prefix(); // includes previous prefix
  new_scope.combine(_formals, args);

  // create input/output tunnels
  if (fromr == tor)
    compound.type = TUNNEL_TYPE;
  tor->add_tunnel(prefix + compound.name, new_prefix + "input", compound.landmark, errh);
  tor->add_tunnel(new_prefix + "output", prefix + compound.name, compound.landmark, errh);
  int new_eindex = tor->eindex(prefix + compound.name);

  int nelements = _elements.size();
  Vector<int> new_fidx(nelements, -1);
  
  // add tunnel pairs and resolve uprefs
  for (int i = 0; i < nelements; i++) {
    ElementClassT *ect = _element_classes[_elements[i].type];
    if (ect)
      new_fidx[i] = ect->expand_into(this, i, tor, new_scope, errh);
    else
      new_fidx[i] = ElementClassT::simple_expand_into(this, i, tor, new_scope, errh);
  }
  
  // add hookup
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hfrom = _hookup_from[i], &hto = _hookup_to[i];
    tor->add_connection(Hookup(new_fidx[hfrom.idx], hfrom.port),
			Hookup(new_fidx[hto.idx], hto.port),
			_hookup_landmark[i]);
  }

  // add requirements
  {
    int thunk = 0, val;
    String key;
    while (_require_map.each(thunk, key, val))
      if (val >= 0)
	tor->add_requirement(key);
  }
  
  // yes, we expanded it
  return new_eindex;
}

void
RouterT::remove_compound_elements(ErrorHandler *errh)
{
  int nelements = _elements.size();
  RouterScope scope;
  for (int i = 0; i < nelements; i++)
    if (_elements[i].type >= 0) { // allow for deleted elements
      ElementClassT *ect = _element_classes[_elements[i].type];
      if (ect)
	ect->expand_into(this, i, this, scope, errh);
      else
	ElementClassT::simple_expand_into(this, i, this, scope, errh);
    }
  
  // remove all compound classes
  int neclass = _element_classes.size();
  Vector<int> removed_eclass(neclass, 0);
  for (int i = 0; i < neclass; i++)
    if (_element_classes[i] && _element_classes[i]->cast_router())
      removed_eclass[i] = -1;
  finish_remove_element_types(removed_eclass);
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


// PRINTING

void
RouterT::compound_declaration_string(StringAccum &sa, const String &name,
				     const String &indent)
{
  sa << indent << "elementclass " << name << " {";
  
  // print formals
  for (int i = 0; i < _formals.size(); i++)
    sa << (i ? ", " : " ") << _formals[i];
  if (_formals.size())
    sa << " |";
  sa << "\n";

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

  // print requirements
  {
    StringAccum require_sa;
    int thunk = 0, val;
    String key;
    while (_require_map.each(thunk, key, val))
      if (val >= 0) {
	if (require_sa.length()) require_sa << ", ";
	require_sa << cp_unsubst(key);
      }
    if (require_sa.length())
      sa << "require(" << require_sa.take_string() << ");\n\n";
  }

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
