// -*- c-basic-offset: 4 -*-
/*
 * routert.{cc,hh} -- tool definition of router
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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
#include "eclasst.hh"
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/variableenv.hh>
#include <stdio.h>

RouterT::RouterT(RouterT *enclosing)
    : _use_count(0), _enclosing_scope(enclosing), _scope_cookie(0),
      _etype_map(-1), _element_name_map(-1),
      _free_element(-1), _real_ecount(0), _new_eindex_collector(0),
      _free_hookup(-1), _archive_map(-1)
{
    // borrow definitions from `enclosing'
    if (enclosing) {
	_enclosing_scope_cookie = _enclosing_scope->_scope_cookie;
	_enclosing_scope->_scope_cookie++;
	_enclosing_scope->use();
    }
}

/*
RouterT::RouterT(const RouterT &o)
  : _use_count(0),
    _element_type_map(o._element_type_map),
    _etypes(o._etypes),
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
  for (int i = 0; i < _etypes.size(); i++)
    if (_etypes[i].eclass)
      _etypes[i].eclass->use();
}
*/

RouterT::~RouterT()
{
  for (int i = 0; i < _etypes.size(); i++)
    if (_etypes[i].eclass)
      _etypes[i].eclass->unuse();
}

void
RouterT::check() const
{
    int ne = nelements();
    int nh = nhookup();
    int nt = _etypes.size();

    // check basic sizes
    assert(_hookup_from.size() == _hookup_to.size()
	   && _hookup_from.size() == _hookup_landmark.size()
	   && _hookup_from.size() == _hookup_next.size());
    assert(_elements.size() == _hookup_first.size());

    // check element type names
    int nt_found = 0;
    for (StringMap::Iterator iter = _etype_map.first(); iter; iter++) {
	int sc = _scope_cookie;
	for (int i = iter.value(); i >= 0; i = _etypes[i].prev_name) {
	    assert(_etypes[i].name() == iter.key());
	    assert(_etypes[i].prev_name < i);
	    assert(_etypes[i].scope_cookie <= sc);
	    sc = _etypes[i].scope_cookie;
	    nt_found++;
	}
    }
    assert(nt_found == nt);

    // check element names
    for (StringMap::Iterator iter = _element_name_map.first(); iter; iter++) {
	String key = iter.key();
	int value = iter.value();
	if (value >= 0)
	    assert(value < ne && _elements[value].name == key); // && _elements[value].live());
    }

    // check free elements
    for (int i = _free_element; i >= 0; i = _elements[i].tunnel_input())
	assert(_elements[i].dead());

    // check elements
    for (int i = 0; i < ne; i++) {
	const ElementT &e = _elements[i];
	if (e.live() && e.tunnel_input() >= 0)
	    assert(e.tunnel() && e.tunnel_input() < ne && _elements[e.tunnel_input()].tunnel_output() == i);
	if (e.live() && e.tunnel_output() >= 0)
	    assert(e.tunnel() && e.tunnel_output() < ne && _elements[e.tunnel_output()].tunnel_input() == i);
    }

    // check hookup
    for (int i = 0; i < nh; i++)
	if (hookup_live(i))
	    assert(has_connection(_hookup_from[i], _hookup_to[i]));

    // check hookup next pointers, port counts
    for (int i = 0; i < ne; i++)
	if (elive(i)) {
	    int ninputs = 0, noutputs = 0;
	    int j = _hookup_first[i].from;
	    while (j >= 0) {
		assert(j < _hookup_from.size());
		assert(_hookup_from[j].idx == i);
		if (_hookup_from[j].port >= noutputs)
		    noutputs = _hookup_from[j].port + 1;
		j = _hookup_next[j].from;
	    }
	    j = _hookup_first[i].to;
	    while (j >= 0) {
		assert(j < _hookup_to.size());
		assert(_hookup_to[j].idx == i && hookup_live(j));
		if (_hookup_to[j].port >= ninputs)
		    ninputs = _hookup_to[j].port + 1;
		j = _hookup_next[j].to;
	    }
	    assert(ninputs == _elements[i].ninputs()
		   && noutputs == _elements[i].noutputs());
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
    for (int i = 0; i < _etypes.size(); i++)
	if (ElementClassT *ec = _etypes[i].eclass)
	    if (ec->cast_router())
		return false;
    return true;
}


ElementClassT *
RouterT::try_type(const String &name) const
{
    int i = _etype_map[name];
    return (i >= 0 ? _etypes[i].eclass : 0);
}

ElementClassT *
RouterT::get_type(ElementClassT *ec, bool install_name = false)
{
    if (install_name && try_type(ec->name()) != ec) {
	int i = _etype_map[ec->name()];
	_etypes.push_back(ElementType(ec, _scope_cookie, i));
	_etype_map.insert(ec->name(), _etypes.size() - 1);
	ec->use();
    }
    return ec;
}

ElementClassT *
RouterT::get_type(const String &name)
{
    int i = _etype_map[name];
    if (i >= 0)
	return _etypes[i].eclass;
    ElementClassT *ec = 0;
    if (_enclosing_scope)
	ec = _enclosing_scope->get_type(name, _enclosing_scope_cookie);
    if (!ec)
	ec = ElementClassT::default_class(name);
    return get_type(ec, true);
}

ElementClassT *
RouterT::get_type(const String &name, int scope_cookie) const
{
    for (int i = _etype_map[name]; i >= 0; i = _etypes[i].prev_name)
	if (_etypes[i].scope_cookie <= scope_cookie)
	    return _etypes[i].eclass;
    if (_enclosing_scope)
	return _enclosing_scope->get_type(name, _enclosing_scope_cookie);
    else
	return 0;
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
	_free_element = _elements[i].tunnel_input();
	_elements[i] = elt;
	_hookup_first[i] = Pair(-1, -1);
    } else {
	i = _elements.size();
	_elements.push_back(elt);
	_hookup_first.push_back(Pair(-1, -1));
    }
    if (_new_eindex_collector)
	_new_eindex_collector->push_back(i);
    _elements[i]._ninputs = _elements[i]._noutputs = 0;
    return i;
}

int
RouterT::get_eindex(const String &name, ElementClassT *type,
		    const String &config, const String &landmark)
{
    int i = _element_name_map[name];
    if (i < 0) {
	i = add_element(ElementT(name, type, config, landmark));
	_element_name_map.insert(name, i);
    }
    return i;
}

int
RouterT::get_anon_eindex(const String &name, ElementClassT *type,
			 const String &config, const String &landmark)
{
    return add_element(ElementT(name, type, config, landmark));
}

int
RouterT::get_anon_eindex(ElementClassT *type, const String &config,
			 const String &landmark)
{
    String name = type->name() + "@" + String(_real_ecount + 1);
    return get_anon_eindex(name, type, config, landmark);
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
RouterT::collect_primitive_classes(HashMap<String, int> &m) const
{
    for (int i = 0; i < _elements.size(); i++)
	_elements[i].type()->collect_primitive_classes(m);
}

void
RouterT::collect_active_types(Vector<ElementClassT *> &v) const
{
    for (StringMap::Iterator iter = _etype_map.first(); iter; iter++)
	v.push_back(_etypes[iter.value()].eclass);
}

void
RouterT::update_noutputs(int e)
{
    int n = 0;
    for (int i = _hookup_first[e].from; i >= 0; i = _hookup_next[i].from)
	if (_hookup_from[i].port >= n)
	    n = _hookup_from[i].port + 1;
    _elements[e]._noutputs = n;
}

void
RouterT::update_ninputs(int e)
{
    int n = 0;
    for (int i = _hookup_first[e].to; i >= 0; i = _hookup_next[i].to)
	if (_hookup_to[i].port >= n)
	    n = _hookup_to[i].port + 1;
    _elements[e]._ninputs = n;
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

    // maintain port counts
    if (hfrom.port >= _elements[hfrom.idx]._noutputs)
	_elements[hfrom.idx]._noutputs = hfrom.port + 1;
    if (hto.port >= _elements[hto.idx]._ninputs)
	_elements[hto.idx]._ninputs = hto.port + 1;

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
	    for (last--; last > i && !hookup_live(last); last--)
		/* nada */;
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

void
RouterT::unlink_connection_from(int c)
{
    int e = _hookup_from[c].idx;
    int port = _hookup_from[c].port;

    // find previous connection
    int prev = -1;
    int trav = _hookup_first[e].from;
    while (trav >= 0 && trav != c) {
	prev = trav;
	trav = _hookup_next[trav].from;
    }
    assert(trav == c);

    // unlink this connection
    if (prev < 0)
	_hookup_first[e].from = _hookup_next[trav].from;
    else
	_hookup_next[prev].from = _hookup_next[trav].from;

    // update port count
    if (_elements[e]._noutputs == port + 1)
	update_noutputs(e);
}

void
RouterT::unlink_connection_to(int c)
{
    int e = _hookup_to[c].idx;
    int port = _hookup_to[c].port;

    // find previous connection
    int prev = -1;
    int trav = _hookup_first[e].to;
    while (trav >= 0 && trav != c) {
	prev = trav;
	trav = _hookup_next[trav].to;
    }
    assert(trav == c);

    // unlink this connection
    if (prev < 0)
	_hookup_first[e].to = _hookup_next[trav].to;
    else
	_hookup_next[prev].to = _hookup_next[trav].to;

    // update port count
    if (_elements[e]._ninputs == port + 1)
	update_ninputs(e);
}

void
RouterT::free_connection(int c)
{
    _hookup_from[c].idx = -1;
    _hookup_next[c].from = _free_hookup;
    _free_hookup = c;
}

void
RouterT::kill_connection(int c)
{
    if (hookup_live(c)) {
	unlink_connection_from(c);
	unlink_connection_to(c);
	free_connection(c);
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
RouterT::change_connection_from(int c, Hookup h)
{
    unlink_connection_from(c);

    _hookup_from[c] = h;
    if (h.port >= _elements[h.idx]._noutputs)
	_elements[h.idx]._noutputs = h.port + 1;

    _hookup_next[c].from = _hookup_first[h.idx].from;
    _hookup_first[h.idx].from = c;
}

void
RouterT::change_connection_to(int c, Hookup h)
{
    unlink_connection_to(c);

    _hookup_to[c] = h;
    if (h.port >= _elements[h.idx]._ninputs)
	_elements[h.idx]._ninputs = h.port + 1;

    _hookup_next[c].to = _hookup_first[h.idx].to;
    _hookup_first[h.idx].to = c;
}

int
RouterT::find_connection(const Hookup &hfrom, const Hookup &hto) const
{
    int c = _hookup_first[hfrom.idx].from;
    while (c >= 0) {
	if (_hookup_from[c] == hfrom && _hookup_to[c] == hto)
	    break;
	c = _hookup_next[c].from;
    }
    return c;
}

bool
RouterT::find_connection_from(const Hookup &h, Hookup &out) const
{
    int c = _hookup_first[h.idx].from;
    int p = h.port;
    out.idx = -1;
    while (c >= 0) {
	if (_hookup_from[c].port == p) {
	    if (out.idx == -1)
		out = _hookup_to[c];
	    else
		out.idx = -2;
	}
	c = _hookup_next[c].from;
    }
    return out.idx >= 0;
}

void
RouterT::find_connections_from(const Hookup &h, Vector<Hookup> &v) const
{
    int c = _hookup_first[h.idx].from;
    int p = h.port;
    while (c >= 0) {
	if (_hookup_from[c].port == p)
	    v.push_back(_hookup_to[c]);
	c = _hookup_next[c].from;
    }
}

void
RouterT::find_connections_from(const Hookup &h, Vector<int> &v) const
{
    int c = _hookup_first[h.idx].from;
    int p = h.port;
    while (c >= 0) {
	if (_hookup_from[c].port == p)
	    v.push_back(c);
	c = _hookup_next[c].from;
    }
}

void
RouterT::find_connections_to(const Hookup &h, Vector<Hookup> &v) const
{
    int c = _hookup_first[h.idx].to;
    int p = h.port;
    while (c >= 0) {
	if (_hookup_to[c].port == p)
	    v.push_back(_hookup_from[c]);
	c = _hookup_next[c].to;
    }
}

void
RouterT::find_connections_to(const Hookup &h, Vector<int> &v) const
{
    int c = _hookup_first[h.idx].to;
    int p = h.port;
    while (c >= 0) {
	if (_hookup_to[c].port == p)
	    v.push_back(c);
	c = _hookup_next[c].to;
    }
}

void
RouterT::find_connection_vector_from(int e, Vector<int> &v) const
{
    v.clear();
    int c = _hookup_first[e].from;
    while (c >= 0) {
	int p = _hookup_from[c].port;
	if (p >= v.size())
	    v.resize(p + 1, -1);
	if (v[p] >= 0)
	    v[p] = -2;
	else
	    v[p] = c;
	c = _hookup_next[c].from;
    }
}

void
RouterT::find_connection_vector_to(int e, Vector<int> &v) const
{
    v.clear();
    int c = _hookup_first[e].to;
    while (c >= 0) {
	int p = _hookup_to[c].port;
	if (p >= v.size())
	    v.resize(p + 1, -1);
	if (v[p] >= 0)
	    v[p] = -2;
	else
	    v[p] = c;
	c = _hookup_next[c].to;
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
    ElementClassT *tun = ElementClassT::tunnel_type();
    int in_idx = get_eindex(in, tun, String(), landmark);
    int out_idx = get_eindex(out, tun, String(), landmark);
    if (!errh) errh = ErrorHandler::silent_handler();
    ElementT &ein = _elements[in_idx], &eout = _elements[out_idx];

    if (!ein.tunnel())
	errh->lerror(landmark, "redeclaration of element `%s'", in.cc());
    else if (!eout.tunnel())
	errh->lerror(landmark, "redeclaration of element `%s'", out.cc());
    else if (ein.tunnel_output() >= 0)
	errh->lerror(landmark, "redeclaration of connection tunnel input `%s'", in.cc());
    else if (eout.tunnel_input() >= 0)
	errh->lerror(landmark, "redeclaration of connection tunnel output `%s'", out.cc());
    else {
	ein._tunnel_output = out_idx;
	eout._tunnel_input = in_idx;
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
    // 5.Dec.1999 - This function dominated the running time of click-xform.
    // Use an algorithm faster on the common case (few connections per
    // element).

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
	if (fac._tunnel_input >= 0)
	    fac._tunnel_input = new_eindex[fac._tunnel_input];
	if (fac._tunnel_output >= 0)
	    fac._tunnel_output = new_eindex[fac._tunnel_output];
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
	    assert(e.dead());
	    e._tunnel_input = _free_element;
	    _free_element = i;
	    _real_ecount--;
	}
}

void
RouterT::free_element(int ei)
{
    // XXX ninputs, noutputs
    
    // first, remove bad connections from other elements' connection lists
    Vector<int> bad_from, bad_to;
    for (int c = _hookup_first[ei].from; c >= 0; c = _hookup_next[c].from)
	unlink_connection_to(c);
    for (int c = _hookup_first[ei].to; c >= 0; c = _hookup_next[c].to)
	unlink_connection_from(c);

    // now, free all of this element's connections
    for (int c = _hookup_first[ei].from; c >= 0; ) {
	int next = _hookup_next[c].from;
	if (_hookup_to[c].idx != ei)
	    free_connection(c);
	c = next;
    }
    for (int c = _hookup_first[ei].to; c >= 0; ) {
	int next = _hookup_next[c].to;
	free_connection(c);
	c = next;
    }
    _hookup_first[ei] = Pair(-1, -1);

    // finally, free the element itself
    ElementT &e = _elements[ei];
    if (_element_name_map[e.name] == ei)
	_element_name_map.insert(e.name, -1);
    e.kill();
    e._tunnel_input = _free_element;
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
    for (int i = _free_element; i >= 0; i = _elements[i].tunnel_input())
	new_eindex[i] = 0;

    finish_free_elements(new_eindex);
}


void
RouterT::expand_into(RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
    assert(tor != this);

    int nelements = _elements.size();
    Vector<int> new_fidx(nelements, -1);

    // add tunnel pairs and expand below
    for (int i = 0; i < nelements; i++)
	new_fidx[i] = ElementClassT::expand_element(this, i, tor, env, errh);

    // add hookup
    int nh = _hookup_from.size();
    for (int i = 0; i < nh; i++) {
	const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
	tor->add_connection(Hookup(new_fidx[hf.idx], hf.port),
			    Hookup(new_fidx[ht.idx], ht.port),
			    _hookup_landmark[i]);
    }

    // add requirements
    for (int i = 0; i < _requirements.size(); i++)
	tor->add_requirement(_requirements[i]);

    // add archive elements
    for (int i = 0; i < _archive.size(); i++) {
	const ArchiveElement &ae = _archive[i];
	if (ae.live() && ae.name != "config") {
	    if (tor->archive_index(ae.name) >= 0)
		errh->error("expansion confict: two archive elements named `%s'", String(ae.name).cc());
	    else
		tor->add_archive(ae);
	}
    }
}

void
RouterT::expand_tunnel(Vector<Hookup> *port_expansions,
		       const Vector<Hookup> &ports,
		       bool is_output, int which,
		       ErrorHandler *errh) const
{
    Vector<Hookup> &expanded = port_expansions[which];

    // quit if circular or already expanded
    if (expanded.size() != 1 || expanded[0].idx != -1)
	return;

    // expand if not expanded yet
    expanded[0].idx = -2;

    const Hookup &me = ports[which];
    int other_idx = (is_output ? _elements[me.idx].tunnel_input() : _elements[me.idx].tunnel_output());
    
    // find connections from tunnel input
    Vector<Hookup> connections;
    if (is_output)
	find_connections_to(Hookup(other_idx, me.port), connections);
    else			// or to tunnel output
	find_connections_from(Hookup(other_idx, me.port), connections);

    // give good errors for unused or nonexistent compound element ports
    if (!connections.size()) {
	int in_idx = (is_output ? other_idx : me.idx);
	int out_idx = (is_output ? me.idx : other_idx);
	String in_name = _elements[in_idx].name;
	String out_name = _elements[out_idx].name;
	if (in_name + "/input" == out_name) {
	    const char *message = (is_output ? "`%s' input %d unused"
				   : "`%s' has no input %d");
	    errh->lerror(_elements[in_idx].landmark(), message, in_name.cc(), me.port);
	} else if (in_name == out_name + "/output") {
	    const char *message = (is_output ? "`%s' has no output %d"
				   : "`%s' output %d unused");
	    errh->lerror(_elements[out_idx].landmark(), message, out_name.cc(), me.port);
	} else {
	    errh->lerror(_elements[other_idx].landmark(),
			 "tunnel `%s -> %s' %s %d unused",
			 in_name.cc(), out_name.cc(),
			 (is_output ? "input" : "output"), me.port);
	}
    }

    // expand them
    Vector<Hookup> store;
    for (int i = 0; i < connections.size(); i++) {
	// if connected to another tunnel, expand that recursively
	if (_elements[connections[i].idx].tunnel()) {
	    int x = connections[i].index_in(ports);
	    if (x >= 0) {
		expand_tunnel(port_expansions, ports, is_output, x, errh);
		const Vector<Hookup> &v = port_expansions[x];
		if (v.size() > 1 || (v.size() == 1 && v[0].idx >= 0))
		    for (int j = 0; j < v.size(); j++)
			store.push_back(v[j]);
		continue;
	    }
	}
	// otherwise, just store it in list of connections
	store.push_back(connections[i]);
    }

    // save results
    expanded.swap(store);
}

void
RouterT::remove_tunnels(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    // find tunnel connections, mark connections by setting index to 'magice'
    Vector<Hookup> inputs, outputs;
    int nhook = _hookup_from.size();
    for (int i = 0; i < nhook; i++) {
	const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
	if (hf.idx < 0)
	    continue;
	if (_elements[hf.idx].tunnel() && _elements[hf.idx].tunnel_input() >= 0)
	    (void) hf.force_index_in(outputs);
	if (_elements[ht.idx].tunnel() && _elements[ht.idx].tunnel_output() >= 0)
	    (void) ht.force_index_in(inputs);
    }

    // expand tunnels
    int nin = inputs.size(), nout = outputs.size();
    Vector<Hookup> *in_expansions = new Vector<Hookup>[nin];
    Vector<Hookup> *out_expansions = new Vector<Hookup>[nout];
    // initialize to placeholders
    for (int i = 0; i < nin; i++)
	in_expansions[i].push_back(Hookup(-1, 0));
    for (int i = 0; i < nout; i++)
	out_expansions[i].push_back(Hookup(-1, 0));
    // actually expand
    for (int i = 0; i < nin; i++)
	expand_tunnel(in_expansions, inputs, false, i, errh);
    for (int i = 0; i < nout; i++)
	expand_tunnel(out_expansions, outputs, true, i, errh);

    // get rid of connections to tunnels
    int nelements = _elements.size();
    int old_nhook = _hookup_from.size();
    for (int i = 0; i < old_nhook; i++) {
	Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];

	// skip if uninteresting
	if (hf.idx < 0 || !_elements[hf.idx].tunnel() || _elements[ht.idx].tunnel())
	    continue;
	int idx = hf.index_in(outputs);
	if (idx < 0)
	    continue;

	// add cross product
	// hf, ht are invalidated by adding new connections!
	Hookup safe_ht = ht;
	String landmark = _hookup_landmark[i]; // must not be reference!
	const Vector<Hookup> &v = out_expansions[idx];
	for (int j = 0; j < v.size(); j++)
	    add_connection(v[j], safe_ht, landmark);
    }
    
    // kill elements with tunnel type
    // but don't kill floating tunnels (like input & output)
    for (int i = 0; i < nelements; i++)
	if (_elements[i].tunnel()
	    && (_elements[i].tunnel_output() >= 0
		|| _elements[i].tunnel_input() >= 0))
	    _elements[i].kill();

    // actually remove tunnel connections and elements
    remove_duplicate_connections();
    free_dead_elements();
}


void
RouterT::remove_compound_elements(ErrorHandler *errh)
{
    int nelements = _elements.size();
    VariableEnvironment env;
    for (int i = 0; i < nelements; i++)
	if (_elements[i].live()) // allow deleted elements
	    ElementClassT::expand_element(this, i, this, env, errh);
}

void
RouterT::flatten(ErrorHandler *errh)
{
    //String s = configuration_string(); fprintf(stderr, "1.\n%s\n\n", s.cc());
    remove_compound_elements(errh);
    //s = configuration_string(); fprintf(stderr, "2.\n%s\n\n", s.cc());
    remove_tunnels(errh);
    //s = configuration_string(); fprintf(stderr, "3.\n%s\n\n", s.cc());
    remove_dead_elements();
    //s = configuration_string(); fprintf(stderr, "4.\n%s\n\n", s.cc());
    compact_connections();
    //s = configuration_string(); fprintf(stderr, "5.\n%s\n\n", s.cc());
    _etype_map.clear();
    _etypes.clear();
    check();
}


// PRINTING

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
RouterT::unparse_requirements(StringAccum &sa, const String &indent) const
{
    if (_requirements.size() > 0) {
	sa << indent << "require(";
	for (int i = 0; i < _requirements.size(); i++) {
	    if (i) sa << ", ";
	    sa << _requirements[i];
	}
	sa << ");\n\n";
    }
}

void
RouterT::unparse_classes(StringAccum &sa, const String &indent) const
{
    int nelemtype = _etypes.size();
    int old_sa_len = sa.length();
    for (int i = 0; i < nelemtype; i++)
	_etypes[i].eclass->unparse_declaration(sa, indent);
    if (sa.length() != old_sa_len)
	sa << "\n";
}

void
RouterT::unparse_declarations(StringAccum &sa, const String &indent) const
{
    int nelements = _elements.size();

    // print tunnel pairs
    int old_sa_len = sa.length();
    for (int i = 0; i < nelements; i++)
	if (_elements[i].tunnel()
	    && _elements[i].tunnel_output() >= 0
	    && _elements[i].tunnel_output() < nelements) {
	    add_line_directive(sa, _elements[i].landmark());
	    sa << indent << "connectiontunnel " << _elements[i].name << " -> "
	       << _elements[ _elements[i].tunnel_output() ].name << ";\n";
	}
    if (sa.length() != old_sa_len)
	sa << "\n";

    // print element declarations
    old_sa_len = sa.length();
    for (int i = 0; i < nelements; i++) {
	const ElementT &e = _elements[i];
	if (e.dead() || e.tunnel())
	    continue; // skip tunnels
	add_line_directive(sa, e.landmark());
	sa << indent << e.name << " :: " << e.type()->name();
	if (e.configuration())
	    sa << "(" << e.configuration() << ")";
	sa << ";\n";
    }
    if (sa.length() != old_sa_len)
	sa << "\n";
}

void
RouterT::unparse_connections(StringAccum &sa, const String &indent) const
{
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
    {
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
	    
	    sa << indent << ename(hf.idx);
	    if (hf.port)
		sa << " [" << hf.port << "]";

	    int d = c;
	    while (d >= 0 && !used[d]) {
		if (d == c)
		    sa << " -> ";
		else
		    sa << "\n" << indent << "    -> ";
		const Hookup &ht = _hookup_to[d];
		if (ht.port)
		    sa << "[" << ht.port << "] ";
		sa << ename(ht.idx);
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

void
RouterT::unparse(StringAccum &sa, const String &indent) const
{
    unparse_requirements(sa, indent);
    unparse_classes(sa, indent);
    unparse_declarations(sa, indent);
    unparse_connections(sa, indent);
}

String
RouterT::configuration_string() const
{
    StringAccum sa;
    unparse(sa);
    return sa.take_string();
}

#include <click/vector.cc>
