/*
 * routert.{cc,hh} -- tool definition of router
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "routert.hh"
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <stdio.h>

RouterT::RouterT(RouterT *enclosing)
  : _enclosing_scope(enclosing),
    _element_type_map(-1), _element_name_map(-1),
    _free_element(-1), _real_ecount(0), _new_eindex_collector(0),
    _free_hookup(-1), _archive_map(-1)
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
    _free_element(o._free_element),
    _real_ecount(o._real_ecount),
    _new_eindex_collector(0),
    _hookup_from(o._hookup_from),
    _hookup_to(o._hookup_to),
    _hookup_landmark(o._hookup_landmark),
    _hookup_next(o._hookup_next),
    _hookup_first(o._hookup_first),
    _free_hookup(o._free_hookup),
    _requirements(o._requirements),
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

void
RouterT::check() const
{
  int ne = nelements();
  int nh = nhookup();
  int nt = ntypes();
  
  // check basic sizes
  assert(_element_type_names.size() == _element_classes.size());
  assert(_hookup_from.size() == _hookup_to.size()
	 && _hookup_from.size() == _hookup_landmark.size()
	 && _hookup_from.size() == _hookup_next.size());
  assert(_elements.size() == _hookup_first.size());

  // check element names
  for (StringMap::Iterator iter = _element_name_map.first(); iter; iter++) {
    String key = iter.key();
    int value = iter.value();
    if (value >= 0)
      assert(value < ne && _elements[value].name == key); // && _elements[value].live());
  }

  // check free elements
  for (int i = _free_element; i >= 0; i = _elements[i].tunnel_input)
    assert(_elements[i].dead());
  
  // check elements
  for (int i = 0; i < ne; i++) {
    const ElementT &e = _elements[i];
    assert(e.dead() || (e.type >= 0 && e.type < nt));
    if (e.live() && e.tunnel_input >= 0)
      assert(e.tunnel_input < ne && _elements[e.tunnel_input].tunnel_output == i);
    if (e.live() && e.tunnel_output >= 0)
      assert(e.tunnel_output < ne && _elements[e.tunnel_output].tunnel_input == i);
  }

  // check hookup
  for (int i = 0; i < nh; i++)
    if (hookup_live(i) && !has_connection(_hookup_from[i], _hookup_to[i])) {
      assert(0);
    }

  // check hookup next pointers
  for (int i = 0; i < ne; i++)
    if (elive(i)) {
      int j = _hookup_first[i].from;
      while (j >= 0) {
	assert(j < _hookup_from.size());
	assert(_hookup_from[j].idx == i);
	j = _hookup_next[j].from;
      }
      j = _hookup_first[i].to;
      while (j >= 0) {
	assert(j < _hookup_to.size());
	assert(_hookup_to[j].idx == i && hookup_live(j));
	j = _hookup_next[j].to;
      }
    }

  // check free hookup pointers
  Bitvector bv(_hookup_from.size(), true);
  for (int i = _free_hookup; i >= 0; i = _hookup_next[i].from) {
    assert(i >= 0 && i < _hookup_from.size());
    assert(bv[i]);
    bv[i] = false;
  }
  for (int i = 0; i < _hookup_from.size(); i++)
    assert(_hookup_from[i].live() == (bool)bv[i]);
}

bool
RouterT::is_flat() const
{
  for (int i = 0; i < _element_classes.size(); i++)
    if (ElementClassT *ec = _element_classes[i])
      if (ec->cast_router())
	return false;
  return true;
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
RouterT::get_type_index(const String &name, ElementClassT *eclass)
{
  int i = _element_type_map[name];
  if (i < 0) {
    i = _element_classes.size();
    _element_type_map.insert(name, i);
    _element_type_names.push_back(name);
    _element_classes.push_back(eclass);
    if (eclass) eclass->use();
  }
  return i;
}

int
RouterT::get_anon_type_index(const String &name, ElementClassT *eclass)
{
  int i = _element_classes.size();
  _element_type_names.push_back(name);
  _element_classes.push_back(eclass);
  if (eclass) eclass->use();
  return i;
}

int
RouterT::add_type_index(const String &name, ElementClassT *eclass)
{
  int i = _element_type_map[name];
  if (i < 0 || _element_classes[i] != eclass) {
    i = _element_classes.size();
    _element_type_map.insert(name, i);
    _element_type_names.push_back(name);
    _element_classes.push_back(eclass);
    if (eclass) eclass->use();
  }
  return i;
}

int
RouterT::add_element(const ElementT &elt)
{
  // don't want to accept an element from ourselves because the vector might
  // grow, invalidating the reference
  assert(_elements.size() == 0 || &elt < &_elements[0] || &elt > &_elements.back());
  
  int i;
  _real_ecount++;
  if (_free_element >= 0) {
    i = _free_element;
    _free_element = _elements[i].tunnel_input;
    _elements[i] = elt;
    _hookup_first[i] = Pair(-1, -1);
  } else {
    i = _elements.size();
    _elements.push_back(elt);
    _hookup_first.push_back(Pair(-1, -1));
  }
  if (_new_eindex_collector)
    _new_eindex_collector->push_back(i);
  return i;
}

int
RouterT::get_eindex(const String &s, int type_index, const String &config,
		    const String &landmark)
{
  int i = _element_name_map[s];
  if (i < 0) {
    assert(type_index >= 0 && type_index < ntypes());
    i = add_element(ElementT(s, type_index, config, landmark));
    _element_name_map.insert(s, i);
  }
  return i;
}

int
RouterT::get_anon_eindex(const String &s, int type_index, const String &config,
			 const String &landmark)
{
  assert(type_index >= 0 && type_index < ntypes());
  return add_element(ElementT(s, type_index, config, landmark));
}

int
RouterT::get_anon_eindex(int type_index, const String &config,
			 const String &landmark)
{
  String name = _element_type_names[type_index] + "@" + String(_real_ecount + 1);
  return get_anon_eindex(name, type_index, config, landmark);
}

void
RouterT::change_ename(int ei, const String &new_name)
{
  ElementT &e = _elements[ei];
  if (e.live()) {
    if (_element_name_map[e.name] == ei)
      _element_name_map.insert(e.name, -1);
    e.name = new_name;
    _element_name_map.insert(new_name, ei);
  }
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
  if (r == this)
    return 0;
  
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
    if (element(i).live())
      element(i).type = new_tidx[ element(i).type ];
  
  return 0;
}


bool
RouterT::add_connection(Hookup hfrom, Hookup hto, const String &landmark)
{
  int ne = _elements.size();
  if (hfrom.idx < 0 || hfrom.idx >= ne || hto.idx < 0 || hto.idx >= ne)
    return false;

  Pair &first_from = _hookup_first[hfrom.idx];
  Pair &first_to = _hookup_first[hto.idx];

  int i;
  if (_free_hookup >= 0) {
    i = _free_hookup;
    _free_hookup = _hookup_next[i].from;
    _hookup_from[i] = hfrom;
    _hookup_to[i] = hto;
    _hookup_landmark[i] = landmark;
    _hookup_next[i] = Pair(first_from.from, first_to.to);
  } else {
    i = _hookup_from.size();
    _hookup_from.push_back(hfrom);
    _hookup_to.push_back(hto);
    _hookup_landmark.push_back(landmark);
    _hookup_next.push_back(Pair(first_from.from, first_to.to));
  }
  first_from.from = first_to.to = i;
  return true;
}

void
RouterT::compact_connections()
{
  int nh = _hookup_from.size();
  Vector<int> new_numbers(nh, -1);
  int last = nh;
  for (int i = 0; i < last; i++)
    if (hookup_live(i))
      new_numbers[i] = i;
    else {
      for (last--; last > i && _hookup_from[last].idx < 0; last--)
	;
      if (last > i)
	new_numbers[last] = i;
    }

  if (last == nh)
    return;
  
  for (int i = 0; i < nh; i++)
    if (new_numbers[i] >= 0 && new_numbers[i] != i) {
      int j = new_numbers[i];
      _hookup_from[j] = _hookup_from[i];
      _hookup_to[j] = _hookup_to[i];
      _hookup_landmark[j] = _hookup_landmark[i];
      _hookup_next[j] = _hookup_next[i];
    }

  _hookup_from.resize(last);
  _hookup_to.resize(last);
  _hookup_landmark.resize(last);
  _hookup_next.resize(last);
  for (int i = 0; i < last; i++) {
    Pair &n = _hookup_next[i];
    if (n.from >= 0) n.from = new_numbers[n.from];
    if (n.to >= 0) n.to = new_numbers[n.to];
  }

  int ne = nelements();
  for (int i = 0; i < ne; i++) {
    Pair &n = _hookup_first[i];
    if (n.from >= 0) n.from = new_numbers[n.from];
    if (n.to >= 0) n.to = new_numbers[n.to];
  }

  _free_hookup = -1;
}

int
RouterT::prev_connection_from(int e, int h) const
{
  int prev = -1;
  int trav = _hookup_first[e].from;
  while (trav >= 0 && trav != h) {
    prev = trav;
    trav = _hookup_next[trav].from;
  }
  return prev;
}

int
RouterT::prev_connection_to(int e, int h) const
{
  int prev = -1;
  int trav = _hookup_first[e].to;
  while (trav >= 0 && trav != h) {
    prev = trav;
    trav = _hookup_next[trav].to;
  }
  return prev;
}

void
RouterT::kill_connection(int i)
{
  if (hookup_live(i)) {
    Hookup &from = _hookup_from[i], &to = _hookup_to[i];
    Pair &next = _hookup_next[i];

    int pfrom = prev_connection_from(from.idx, i);
    if (pfrom >= 0)
      _hookup_next[pfrom].from = next.from;
    else
      _hookup_first[ from.idx ].from = next.from;
    
    int pto = prev_connection_to(to.idx, i);
    if (pto >= 0)
      _hookup_next[pto].to = next.to;
    else
      _hookup_first[ to.idx ].to = next.to;
    
    from.idx = -1;
    next.from = _free_hookup;
    _free_hookup = i;
  }
}

void
RouterT::kill_bad_connections()
{
  int nh = nhookup();
  Vector<Hookup> &hf = _hookup_from;
  Vector<Hookup> &ht = _hookup_to;
  for (int i = 0; i < nh; i++)
    if (hf[i].live() && (edead(hf[i].idx) || edead(ht[i].idx)))
      kill_connection(i);
}

void
RouterT::change_connection_from(int i, Hookup h)
{
  Hookup &from = _hookup_from[i];
  Pair &next = _hookup_next[i];
  int pfrom = prev_connection_from(from.idx, i);
  if (pfrom >= 0)
    _hookup_next[pfrom].from = next.from;
  else
    _hookup_first[ from.idx ].from = next.from;
  
  _hookup_from[i] = h;
  
  next.from = _hookup_first[ h.idx ].from;
  _hookup_first[ h.idx ].from = i;
}

void
RouterT::change_connection_to(int i, Hookup h)
{
  Hookup &to = _hookup_to[i];
  Pair &next = _hookup_next[i];
  int pto = prev_connection_to(to.idx, i);
  if (pto >= 0)
    _hookup_next[pto].to = next.to;
  else
    _hookup_first[ to.idx ].to = next.to;
  
  _hookup_to[i] = h;
  
  next.to = _hookup_first[ h.idx ].to;
  _hookup_first[ h.idx ].to = i;
}

int
RouterT::find_connection(const Hookup &hfrom, const Hookup &hto) const
{
  int i = _hookup_first[hfrom.idx].from;
  while (i >= 0) {
    if (_hookup_from[i] == hfrom && _hookup_to[i] == hto)
      break;
    i = _hookup_next[i].from;
  }
  return i;
}

bool
RouterT::find_connection_from(const Hookup &h, Hookup &out) const
{
  int i = _hookup_first[h.idx].from;
  int p = h.port;
  out.idx = -1;
  while (i >= 0) {
    if (_hookup_from[i].port == p) {
      if (out.idx == -1)
	out = _hookup_to[i];
      else
	out.idx = -2;
    }
    i = _hookup_next[i].from;
  }
  return out.idx >= 0;
}

void
RouterT::find_connections_from(const Hookup &h, Vector<Hookup> &v) const
{
  int i = _hookup_first[h.idx].from;
  int p = h.port;
  while (i >= 0) {
    if (_hookup_from[i].port == p)
      v.push_back(_hookup_to[i]);
    i = _hookup_next[i].from;
  }
}

void
RouterT::find_connections_from(const Hookup &h, Vector<int> &v) const
{
  int i = _hookup_first[h.idx].from;
  int p = h.port;
  while (i >= 0) {
    if (_hookup_from[i].port == p)
      v.push_back(i);
    i = _hookup_next[i].from;
  }
}

void
RouterT::find_connections_to(const Hookup &h, Vector<Hookup> &v) const
{
  int i = _hookup_first[h.idx].to;
  int p = h.port;
  while (i >= 0) {
    if (_hookup_to[i].port == p)
      v.push_back(_hookup_from[i]);
    i = _hookup_next[i].to;
  }
}

void
RouterT::find_connections_to(const Hookup &h, Vector<int> &v) const
{
  int i = _hookup_first[h.idx].to;
  int p = h.port;
  while (i >= 0) {
    if (_hookup_to[i].port == p)
      v.push_back(i);
    i = _hookup_next[i].to;
  }
}

void
RouterT::find_connection_vector_from(int e, Vector<int> &v) const
{
  v.clear();
  int i = _hookup_first[e].from;
  while (i >= 0) {
    int p = _hookup_from[i].port;
    if (p >= v.size())
      v.resize(p + 1, -1);
    if (v[p] >= 0)
      v[p] = -2;
    else
      v[p] = i;
    i = _hookup_next[i].from;
  }
}

void
RouterT::find_connection_vector_to(int e, Vector<int> &v) const
{
  v.clear();
  int i = _hookup_first[e].to;
  while (i >= 0) {
    int p = _hookup_to[i].port;
    if (p >= v.size())
      v.resize(p + 1, -1);
    if (v[p] >= 0)
      v[p] = -2;
    else
      v[p] = i;
    i = _hookup_next[i].to;
  }
}

void
RouterT::count_ports(Vector<int> &inputs, Vector<int> &outputs) const
{
  inputs.assign(_elements.size(), 0);
  outputs.assign(_elements.size(), 0);
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    if (hf.dead())
      continue;
    if (hf.port >= outputs[hf.idx])
      outputs[hf.idx] = hf.port + 1;
    if (ht.port >= inputs[ht.idx])
      inputs[ht.idx] = ht.port + 1;
  }
}

int
RouterT::ninputs(int e) const
{
  int n = 0;
  int i = _hookup_first[e].to;
  while (i >= 0) {
    if (_hookup_to[i].port >= n)
      n = _hookup_to[i].port + 1;
    i = _hookup_next[i].to;
  }
  return n;
}

int
RouterT::noutputs(int e) const
{
  int n = 0;
  int i = _hookup_first[e].from;
  while (i >= 0) {
    if (_hookup_from[i].port >= n)
      n = _hookup_from[i].port + 1;
    i = _hookup_next[i].from;
  }
  return n;
}

bool
RouterT::insert_before(const Hookup &inserter, const Hookup &h)
{
  if (!add_connection(inserter, h))
    return false;
  
  int i = _hookup_first[h.idx].to;
  while (i >= 0) {
    int next = _hookup_next[i].to;
    if (_hookup_to[i] == h && hookup_live(i) && _hookup_from[i] != inserter)
      change_connection_to(i, inserter);
    i = next;
  }
  return true;
}

bool
RouterT::insert_after(const Hookup &inserter, const Hookup &h)
{
  if (!add_connection(h, inserter))
    return false;
  
  int i = _hookup_first[h.idx].from;
  while (i >= 0) {
    int next = _hookup_next[i].from;
    if (_hookup_from[i] == h && _hookup_to[i] != inserter)
      change_connection_from(i, inserter);
    i = next;
  }
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
    errh->lerror(landmark, "redeclaration of element `%s'", in.cc());
  else if (fout.type != TUNNEL_TYPE)
    errh->lerror(landmark, "redeclaration of element `%s'", out.cc());
  else if (fin.tunnel_output >= 0)
    errh->lerror(landmark, "redeclaration of connection tunnel input `%s'", in.cc());
  else if (fout.tunnel_input >= 0)
    errh->lerror(landmark, "redeclaration of connection tunnel output `%s'", out.cc());
  else {
    fin.tunnel_output = out_idx;
    fout.tunnel_input = in_idx;
  }
}


void
RouterT::add_requirement(const String &s)
{
  _requirements.push_back(s);
}

void
RouterT::remove_requirement(const String &s)
{
  for (int i = 0; i < _requirements.size(); i++)
    if (_requirements[i] == s) {
      // keep requirements in order
      for (int j = i + 1; j < _requirements.size(); j++)
	_requirements[j-1] = _requirements[j];
      _requirements.pop_back();
      return;
    }
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
RouterT::remove_duplicate_connections()
{
  // 5.Dec.1999 - This function dominated the running time of click-xform. Use
  // an algorithm faster on the common case (few connections per element).

  int nelem = _elements.size();
  Vector<int> removers;

  for (int i = 0; i < nelem; i++) {
    int trav = _hookup_first[i].from;
    int next = 0;		// initialize here to avoid gcc warnings
    while (trav >= 0) {
      int prev = _hookup_first[i].from;
      int trav_port = _hookup_from[trav].port;
      next = _hookup_next[trav].from;
      while (prev >= 0 && prev != trav) {
	if (_hookup_from[prev].port == trav_port
	    && _hookup_to[prev] == _hookup_to[trav]) {
	  kill_connection(trav);
	  goto duplicate;
	}
	prev = _hookup_next[prev].from;
      }
     duplicate:
      trav = next;
    }
  }
}


void
RouterT::finish_remove_elements(Vector<int> &new_eindex, ErrorHandler *errh)
{
  if (!errh) errh = ErrorHandler::silent_handler();
  int nelements = _elements.size();

  // find new findexes
  int j = 0;
  for (int i = 0; i < nelements; i++)
    if (new_eindex[i] >= 0)
      new_eindex[i] = j++;
  int new_nelements = j;
  
  // change hookup
  int nh = _hookup_from.size();
  for (int i = 0; i < nh; i++) {
    Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    bool bad = false;
    if (hf.idx < 0 || hf.idx >= nelements || ht.idx < 0 || ht.idx >= nelements)
      bad = true;
    else if (new_eindex[hf.idx] < 0) {
      errh->lerror(_hookup_landmark[i], "connection from removed element `%s'", ename(hf.idx).cc());
      bad = true;
    } else if (new_eindex[ht.idx] < 0) {
      errh->lerror(_hookup_landmark[i], "connection to removed element `%s'", ename(ht.idx).cc());
      bad = true;
    }
    
    if (!bad) {
      hf.idx = new_eindex[hf.idx];
      ht.idx = new_eindex[ht.idx];
    } else if (hf.idx >= 0)
      kill_connection(i);
  }

  // compress element arrays
  for (int i = 0; i < nelements; i++) {
    j = new_eindex[i];
    if (j != i) {
      if (_elements[i].type != UPREF_TYPE)
	_element_name_map.insert(_elements[i].name, j);
      if (j >= 0) {
	_elements[j] = _elements[i];
	_hookup_first[j] = _hookup_first[i];
      }
    }
  }

  // massage tunnel pointers
  for (int i = 0; i < new_nelements; i++) {
    ElementT &fac = _elements[i];
    if (fac.tunnel_input >= 0)
      fac.tunnel_input = new_eindex[fac.tunnel_input];
    if (fac.tunnel_output >= 0)
      fac.tunnel_output = new_eindex[fac.tunnel_output];
  }

  // resize element arrays
  _elements.resize(new_nelements);
  _hookup_first.resize(new_nelements);
  _real_ecount = new_nelements;
  _free_element = -1;
}

void
RouterT::remove_dead_elements(ErrorHandler *errh = 0)
{
  int nelements = _elements.size();

  // mark saved findexes
  Vector<int> new_eindex(nelements, 0);
  for (int i = 0; i < nelements; i++)
    if (_elements[i].dead())
      new_eindex[i] = -1;

  finish_remove_elements(new_eindex, errh);
}

void
RouterT::finish_free_elements(Vector<int> &new_eindex)
{
  int nelements = _elements.size();

  // change hookup
  for (int i = 0; i < _hookup_from.size(); i++) {
    Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    bool bad = false;
    if (hf.idx < 0 || hf.idx >= nelements || ht.idx < 0 || ht.idx >= nelements)
      bad = true;
    else if (new_eindex[hf.idx] < 0 || new_eindex[ht.idx] < 0)
      bad = true;
    if (bad)
      kill_connection(i);
  }

  // free elements
  for (int i = 0; i < nelements; i++)
    if (new_eindex[i] < 0) {
      ElementT &e = _elements[i];
      if (_element_name_map[e.name] == i)
	_element_name_map.insert(e.name, -1);
      assert(e.type < 0);
      e.type = -1;
      e.tunnel_input = _free_element;
      _free_element = i;
      _real_ecount--;
    }
}

void
RouterT::free_element(int ei)
{
  // first, remove bad connections from other elements' connection lists
  Vector<int> bad_from, bad_to;
  for (int c = _hookup_first[ei].from; c >= 0; c = _hookup_next[c].from)
    bad_to.push_back(_hookup_to[c].idx);
  for (int c = _hookup_first[ei].to; c >= 0; c = _hookup_next[c].to)
    bad_from.push_back(_hookup_from[c].idx);

  for (int i = 0; i < bad_from.size(); i++)
    if (bad_from[i] != ei)
      for (int *cp = &_hookup_first[bad_from[i]].from; *cp >= 0; ) {
	if (_hookup_to[*cp].idx == ei)
	  *cp = _hookup_next[*cp].from;
	else
	  cp = &_hookup_next[*cp].from;
      }
  for (int i = 0; i < bad_to.size(); i++)
    if (bad_to[i] != ei)
      for (int *cp = &_hookup_first[bad_to[i]].to; *cp >= 0; ) {
	if (_hookup_from[*cp].idx == ei)
	  *cp = _hookup_next[*cp].to;
	else
	  cp = &_hookup_next[*cp].to;
      }

  // now, free all of this element's connections
  for (int c = _hookup_first[ei].from; c >= 0; ) {
    int next = _hookup_next[c].from;
    if (_hookup_to[c].idx != ei) {
      _hookup_from[c].idx = -1;
      _hookup_next[c].from = _free_hookup;
      _free_hookup = c;
    }
    c = next;
  }
  for (int c = _hookup_first[ei].to; c >= 0; ) {
    int next = _hookup_next[c].to;
    _hookup_from[c].idx = -1;
    _hookup_next[c].from = _free_hookup;
    _free_hookup = c;
    c = next;
  }
  _hookup_first[ei] = Pair(-1, -1);

  // finally, free the element itself
  ElementT &e = _elements[ei];
  if (_element_name_map[e.name] == ei)
    _element_name_map.insert(e.name, -1);
  e.type = -1;
  e.tunnel_input = _free_element;
  _free_element = ei;
  _real_ecount--;

  check();
}

void
RouterT::free_dead_elements()
{
  int nelements = _elements.size();

  // mark saved findexes
  Vector<int> new_eindex(nelements, 0);
  for (int i = 0; i < nelements; i++)
    if (_elements[i].dead())
      new_eindex[i] = -1;

  // don't free elements that have already been freed!!
  for (int i = _free_element; i >= 0; i = _elements[i].tunnel_input)
    new_eindex[i] = 0;

  finish_free_elements(new_eindex);
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
		       bool is_input, int magice, int which,
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
      find_connections_from(Hookup(magice, which), cur_expansion);
    else			// or to corresponding tunnel input
      find_connections_to(Hookup(magice, which), cur_expansion);
    // expand them
    for (int i = 0; i < cur_expansion.size(); i++)
      if (cur_expansion[i].idx == magice)
	expand_tunnel(pp_expansions, is_input, magice, cur_expansion[i].port,
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
RouterT::remove_tunnels()
{
  // add element
  int magice = get_anon_eindex(TUNNEL_TYPE);
  
  // find tunnel connections, mark connections by setting index to -99
  Vector<Hookup> tunnel_in, tunnel_out;
  int nhook = _hookup_from.size();
  for (int i = 0; i < nhook; i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    if (hf.idx < 0)
      continue;
    int pp_in = _elements[hf.idx].tunnel_input;
    int pp_out = _elements[ht.idx].tunnel_output;
    if (pp_in >= 0) {
      int j = hf.index_in(tunnel_out);
      if (j < 0) {
	tunnel_in.push_back(Hookup(pp_in, hf.port));
	tunnel_out.push_back(hf);
	j = tunnel_in.size() - 1;
      }
      change_connection_from(i, Hookup(magice, j));
    }
    if (pp_out >= 0) {
      int j = ht.index_in(tunnel_in);
      if (j < 0) {
	tunnel_in.push_back(ht);
	tunnel_out.push_back(Hookup(pp_out, ht.port));
	j = tunnel_in.size() - 1;
      }
      change_connection_to(i, Hookup(magice, j));
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
    Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    String landmark = _hookup_landmark[i]; // must not be reference!
    
    // skip if uninteresting
    if (hf.idx != magice)
      continue;
    
    // find first-level connections
    Vector<Hookup> new_from, new_to;
    if (hf.idx == magice)
      expand_tunnel(ppout_expansions, false, magice, hf.port, new_from);
    else
      new_from.push_back(hf);
    if (ht.idx == magice)
      expand_tunnel(ppin_expansions, true, magice, ht.port, new_to);
    else
      new_to.push_back(ht);
    
    // add cross product
    // hf, ht are invalidated by adding new connections!
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
  _elements[magice].type = -1;

  // actually remove tunnel connections and elements
  remove_duplicate_connections();
  free_dead_elements();
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
  int quote = 0;
  String output;
  
  for (; pos < len; pos++)
    if (data[pos] == '\\' && pos < len - 1 && quote == '\"')
      pos++;
    else if (data[pos] == '\'' && quote == 0)
      quote = '\'';
    else if (data[pos] == '\"' && quote == 0)
      quote = '\"';
    else if (data[pos] == quote)
      quote = 0;
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
    } else if (data[pos] == '$' && quote != '\'') {
      unsigned word_pos = pos;
      for (pos++; isalnum(data[pos]) || data[pos] == '_'; pos++)
	/* nada */;
      String name = config.substring(word_pos, pos - word_pos);
      for (int variable = 0; variable < _formals.size(); variable++)
	if (name == _formals[variable]) {
	  output += config.substring(config_pos, word_pos - config_pos);
	  String value = _values[variable];
	  if (quote == '\"') {	// interpolate inside the quotes
	    value = cp_quote(cp_unquote(value));
	    if (value[0] == '\"')
	      value = value.substring(1, value.length() - 2);
	  }
	  output += value;
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
  // must make a copy of `compound' because we might be growing the _elements
  // vector, in which case our reference would die
  ElementT compound = fromr->element(which);
  
  // parse configuration string
  Vector<String> args;
  int nargs = _formals.size();
  cp_argvec(scope.interpolate(compound.configuration), args);
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
    tor->element(which).type = TUNNEL_TYPE;
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
  for (int i = 0; i < _requirements.size(); i++)
    tor->add_requirement(_requirements[i]);
  
  // yes, we expanded it
  return new_eindex;
}

void
RouterT::remove_compound_elements(ErrorHandler *errh)
{
  int nelements = _elements.size();
  RouterScope scope;
  for (int i = 0; i < nelements; i++)
    if (_elements[i].live()) {	// allow for deleted elements
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
    if (_element_classes[i] && _element_classes[i]->expands_away())
      removed_eclass[i] = -1;
  finish_remove_element_types(removed_eclass);
}

void
RouterT::remove_unresolved_uprefs(ErrorHandler *errh)
{
  if (!errh) errh = ErrorHandler::silent_handler();
  
  int nelements = _elements.size();
  Vector<int> new_eindex(nelements, 0);
  bool any = false;
  
  // find uprefs
  for (int i = 0; i < nelements; i++) {
    ElementT &e = _elements[i];
    if (e.type == UPREF_TYPE) {
      errh->lerror(e.landmark, "unresolved upref `%s'", e.name.cc());
      new_eindex[i] = -1;
      any = true;
    }
  }

  if (any)
    finish_free_elements(new_eindex);
}

void
RouterT::flatten(ErrorHandler *errh)
{
  remove_compound_elements(errh);
  remove_tunnels();
  remove_unresolved_uprefs(errh);
  remove_dead_elements();
  compact_connections();
  check();
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

static void
add_line_directive(StringAccum &sa, const String &landmark)
{
  int colon = landmark.find_right(':');
  // XXX protect filename
  if (colon >= 0)
    sa << "# " << landmark.substring(colon + 1) << " \""
       << landmark.substring(0, colon) << "\"\n";
  else
    sa << "# 0 \"" << landmark << "\"\n";
}

void
RouterT::configuration_string(StringAccum &sa, const String &indent) const
{
  int nelements = _elements.size();
  int nelemtype = _element_classes.size();

  // print requirements
  if (_requirements.size() > 0) {
    sa << "require(";
    for (int i = 0; i < _requirements.size(); i++) {
      if (i) sa << ", ";
      sa << _requirements[i];
    }
    sa << ");\n\n";
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
      add_line_directive(sa, _elements[i].landmark);
      sa << indent << "connectiontunnel " << _elements[i].name << " -> "
	 << _elements[ _elements[i].tunnel_output ].name << ";\n";
    }
  if (sa.length() != old_sa_len)
    sa << "\n";
  
  // print element declarations
  int nprinted_elements = 0;
  for (int i = 0; i < nelements; i++) {
    const ElementT &e = _elements[i];
    if (e.dead() || e.type == TUNNEL_TYPE || e.type == UPREF_TYPE)
      continue; // skip tunnels and uprefs
    add_line_directive(sa, e.landmark);
    sa << indent << e.name << " :: ";
    if (e.type < nelemtype)
      sa << _element_type_names[e.type];
    else
      sa << "Error /*BAD_TYPE_" << e.type << "*/";
    if (e.configuration)
      sa << "(" << e.configuration << ")";
    sa << ";\n";
    nprinted_elements++;
  }
  if (nprinted_elements)
    sa << "\n";

  // mark loser connections
  int nhookup = _hookup_from.size();
  Bitvector used(nhookup, false);
  for (int c = 0; c < nhookup; c++)
    if (_hookup_from[c].idx < 0)
      used[c] = true;
  
  // prepare hookup chains
  Vector<int> next(nhookup, -1);
  Bitvector startchain(nhookup, true);
  for (int c = 0; c < nhookup; c++) {
    const Hookup &ht = _hookup_to[c];
    if (ht.port != 0 || used[c]) continue;
    int result = -1;
    for (int d = 0; d < nhookup; d++)
      if (d != c && _hookup_from[d] == ht && !used[d]) {
	result = d;
	if (_hookup_to[d].port == 0)
	  break;
      }
    if (result >= 0) {
      next[c] = result;
      startchain[result] = false;
    }
  }

  // count line numbers so we can give reasonable error messages
  if (nprinted_elements) {
    int lineno = 1;
    const char *s = sa.data();
    int len = sa.length();
    for (int i = 0; i < len; i++)
      if (s[i] == '\n')
	lineno++;
    sa << "# " << lineno + 1 << " \"\"\n";
  }
  
  // print hookup
  bool done = false;
  while (!done) {
    // print chains
    for (int c = 0; c < nhookup; c++) {
      const Hookup &hf = _hookup_from[c];
      if (used[c] || !startchain[c])
	continue;
      
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
