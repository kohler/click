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

RouterT::RouterT(ElementClassT *type, RouterT *enclosing_scope)
    : _use_count(0), _enclosing_type(type),
      _enclosing_scope(enclosing_scope), _scope_cookie(0),
      _etype_map(-1), _element_name_map(-1),
      _free_element(0), _real_ecount(0), _new_eindex_collector(0),
      _free_conn(-1), _archive_map(-1)
{
    // borrow definitions from `enclosing'
    if (_enclosing_scope) {
	_enclosing_scope_cookie = _enclosing_scope->_scope_cookie;
	_enclosing_scope->_scope_cookie++;
	_enclosing_scope->use();
    }
}

RouterT::~RouterT()
{
  for (int i = 0; i < _etypes.size(); i++)
    if (_etypes[i].eclass)
      _etypes[i].eclass->unuse();
  for (int i = 0; i < _elements.size(); i++)
      delete _elements[i];
}

void
RouterT::check() const
{
    int ne = nelements();
    int nc = nconnections();
    int nt = _etypes.size();

    // check basic sizes
    assert(_elements.size() == _first_conn.size());

    // check element type names
    int nt_found = 0;
    for (StringMap::iterator iter = _etype_map.begin(); iter; iter++) {
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
    for (StringMap::iterator iter = _element_name_map.begin(); iter; iter++) {
	String key = iter.key();
	int value = iter.value();
	if (value >= 0)
	    assert(value < ne && ename(value) == key); // && _elements[value].live());
    }

    // check free elements
    for (ElementT *e = _free_element; e; e = e->tunnel_input()) {
	assert(e->dead());
	assert(_elements[e->idx()] == e);
    }

    // check elements
    for (int i = 0; i < ne; i++) {
	const ElementT *e = _elements[i];
	assert(e->router() == this);
	if (e->live() && e->tunnel_input())
	    assert(e->tunnel() && e->tunnel_input()->tunnel_output() == e);
	if (e->live() && e->tunnel_output())
	    assert(e->tunnel() && e->tunnel_output()->tunnel_input() == e);
    }

    // check hookup
    for (int i = 0; i < nc; i++)
	if (connection_live(i))
	    assert(has_connection(_conn[i].from(), _conn[i].to()));

    // check hookup next pointers, port counts
    for (int i = 0; i < ne; i++)
	if (elive(i)) {
	    int ninputs = 0, noutputs = 0;
	    const ElementT *e = elt(i);
	    
	    int j = _first_conn[i].from;
	    while (j >= 0) {
		assert(j < _conn.size());
		assert(_conn[j].from_elt() == e);
		if (_conn[j].from().port >= noutputs)
		    noutputs = _conn[j].from().port + 1;
		j = _conn[j].next_from();
	    }
	    
	    j = _first_conn[i].to;
	    while (j >= 0) {
		assert(j < _conn.size());
		assert(_conn[j].to_elt() == e && connection_live(j));
		if (_conn[j].to().port >= ninputs)
		    ninputs = _conn[j].to().port + 1;
		j = _conn[j].next_to();
	    }
	    
	    assert(ninputs == e->ninputs() && noutputs == e->noutputs());
	}

    // check free hookup pointers
    Bitvector bv(_conn.size(), true);
    for (int i = _free_conn; i >= 0; i = _conn[i].next_from()) {
	assert(i >= 0 && i < _conn.size());
	assert(bv[i]);
	bv[i] = false;
    }
    for (int i = 0; i < _conn.size(); i++)
	assert(connection_live(i) == (bool)bv[i]);
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
RouterT::get_type(ElementClassT *ec, bool install_name)
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

ElementT *
RouterT::add_element(const ElementT &elt_in)
{
    int i;
    _real_ecount++;
    ElementT *elt = new ElementT(elt_in);
    if (_free_element) {
	i = _free_element->idx();
	_free_element = _free_element->tunnel_input();
	delete _elements[i];
	_first_conn[i] = Pair(-1, -1);
    } else {
	i = _elements.size();
	_elements.push_back(0);
	_first_conn.push_back(Pair(-1, -1));
    }
    if (_new_eindex_collector)
	_new_eindex_collector->push_back(i);
    _elements[i] = elt;
    elt->_idx = i;
    elt->_owner = this;
    return elt;
}

ElementT *
RouterT::get_element(const String &name, ElementClassT *type,
		     const String &config, const String &landmark)
{
    int i = _element_name_map[name];
    if (i >= 0)
	return _elements[i];
    else {
	ElementT *e = add_element(ElementT(name, type, config, landmark));
	_element_name_map.insert(name, e->idx());
	return e;
    }
}

ElementT *
RouterT::add_anon_element(ElementClassT *type, const String &config,
			  const String &landmark)
{
    String name = type->name() + "@" + String(_real_ecount + 1);
    return add_element(ElementT(name, type, config, landmark));
}

void
RouterT::change_ename(int ei, const String &new_name)
{
    assert(new_name);
    ElementT &e = *_elements[ei];
    if (e.live()) {
	if (_element_name_map[e.name()] == ei)
	    _element_name_map.insert(e.name(), -1);
	e._name = new_name;
	_element_name_map.insert(new_name, ei);
    }
}

void
RouterT::collect_primitive_classes(HashMap<String, int> &m) const
{
    for (int i = 0; i < _elements.size(); i++)
	_elements[i]->type()->collect_primitive_classes(m);
}

void
RouterT::collect_active_types(Vector<ElementClassT *> &v) const
{
    for (StringMap::iterator iter = _etype_map.begin(); iter; iter++)
	v.push_back(_etypes[iter.value()].eclass);
}

void
RouterT::update_noutputs(int e)
{
    int n = 0;
    for (int i = _first_conn[e].from; i >= 0; i = _conn[i].next_from())
	if (_conn[i].from().port >= n)
	    n = _conn[i].from().port + 1;
    _elements[e]->_noutputs = n;
}

void
RouterT::update_ninputs(int e)
{
    int n = 0;
    for (int i = _first_conn[e].to; i >= 0; i = _conn[i].next_to())
	if (_conn[i].to().port >= n)
	    n = _conn[i].to().port + 1;
    _elements[e]->_ninputs = n;
}

bool
RouterT::add_connection(const PortT &hfrom, const PortT &hto,
			const String &landmark)
{
    assert(hfrom.router() == this && hfrom.elt->live()
	   && hto.router() == this && hto.elt->live());

    Pair &first_from = _first_conn[hfrom.idx()];
    Pair &first_to = _first_conn[hto.idx()];

    int i;
    if (_free_conn >= 0) {
	i = _free_conn;
	_free_conn = _conn[i].next_from();
	_conn[i] = ConnectionT(hfrom, hto, landmark, first_from.from, first_to.to);
    } else {
	i = _conn.size();
	_conn.push_back(ConnectionT(hfrom, hto, landmark, first_from.from, first_to.to));
    }
    
    first_from.from = first_to.to = i;

    // maintain port counts
    if (hfrom.port >= hfrom.elt->_noutputs)
	hfrom.elt->_noutputs = hfrom.port + 1;
    if (hto.port >= hto.elt->_ninputs)
	hto.elt->_ninputs = hto.port + 1;

    return true;
}

void
RouterT::compact_connections()
{
    int nc = _conn.size();
    Vector<int> new_numbers(nc, -1);
    int last = nc;
    for (int i = 0; i < last; i++)
	if (connection_live(i))
	    new_numbers[i] = i;
	else {
	    for (last--; last > i && !connection_live(last); last--)
		/* nada */;
	    if (last > i)
		new_numbers[last] = i;
	}

    if (last == nc)
	return;

    for (int i = 0; i < nc; i++)
	if (new_numbers[i] >= 0 && new_numbers[i] != i)
	    _conn[ new_numbers[i] ] = _conn[i];

    _conn.resize(last);
    for (int i = 0; i < last; i++) {
	ConnectionT &c = _conn[i];
	if (c._next_from >= 0)
	    c._next_from = new_numbers[c._next_from];
	if (c._next_to >= 0)
	    c._next_to = new_numbers[c._next_to];
    }

    int ne = nelements();
    for (int i = 0; i < ne; i++) {
	Pair &n = _first_conn[i];
	if (n.from >= 0)
	    n.from = new_numbers[n.from];
	if (n.to >= 0)
	    n.to = new_numbers[n.to];
    }

    _free_conn = -1;
}

void
RouterT::unlink_connection_from(int c)
{
    int e = _conn[c].from_idx();
    int port = _conn[c].from_port();

    // find previous connection
    int prev = -1;
    int trav = _first_conn[e].from;
    while (trav >= 0 && trav != c) {
	prev = trav;
	trav = _conn[trav].next_from();
    }
    assert(trav == c);

    // unlink this connection
    if (prev < 0)
	_first_conn[e].from = _conn[trav].next_from();
    else
	_conn[prev]._next_from = _conn[trav].next_from();

    // update port count
    if (_elements[e]->_noutputs == port + 1)
	update_noutputs(e);
}

void
RouterT::unlink_connection_to(int c)
{
    int e = _conn[c].to_idx();
    int port = _conn[c].to_port();

    // find previous connection
    int prev = -1;
    int trav = _first_conn[e].to;
    while (trav >= 0 && trav != c) {
	prev = trav;
	trav = _conn[trav].next_to();
    }
    assert(trav == c);

    // unlink this connection
    if (prev < 0)
	_first_conn[e].to = _conn[trav].next_to();
    else
	_conn[prev]._next_to = _conn[trav].next_to();

    // update port count
    if (_elements[e]->_ninputs == port + 1)
	update_ninputs(e);
}

void
RouterT::free_connection(int c)
{
    _conn[c].kill();
    _conn[c]._next_from = _free_conn;
    _free_conn = c;
}

void
RouterT::kill_connection(int c)
{
    if (connection_live(c)) {
	unlink_connection_from(c);
	unlink_connection_to(c);
	free_connection(c);
    }
}

void
RouterT::kill_bad_connections()
{
    int nc = nconnections();
    for (int i = 0; i < nc; i++) {
	ConnectionT &c = _conn[i];
	if (c.live() && (c.from_elt()->dead() || c.to_elt()->dead()))
	    kill_connection(i);
    }
}

void
RouterT::change_connection_from(int c, PortT h)
{
    assert(h.router() == this);
    unlink_connection_from(c);

    _conn[c]._from = h;
    if (h.port >= h.elt->_noutputs)
	h.elt->_noutputs = h.port + 1;

    int ei = h.idx();
    _conn[c]._next_from = _first_conn[ei].from;
    _first_conn[ei].from = c;
}

void
RouterT::change_connection_to(int c, PortT h)
{
    assert(h.router() == this);
    unlink_connection_to(c);

    _conn[c]._to = h;
    if (h.port >= h.elt->_ninputs)
	h.elt->_ninputs = h.port + 1;

    int ei = h.idx();
    _conn[c]._next_to = _first_conn[ei].to;
    _first_conn[ei].to = c;
}

int
RouterT::find_connection(const PortT &hfrom, const PortT &hto) const
{
    assert(hfrom.router() == this && hto.router() == this);
    int c = _first_conn[hfrom.idx()].from;
    while (c >= 0) {
	if (_conn[c].from() == hfrom && _conn[c].to() == hto)
	    break;
	c = _conn[c].next_from();
    }
    return c;
}

bool
RouterT::find_connection_from(const PortT &h, PortT &out) const
{
    assert(h.router() == this);
    int c = _first_conn[h.idx()].from;
    int p = h.port;
    out = PortT(0, -1);
    while (c >= 0) {
	if (_conn[c].from_port() == p) {
	    if (out.port == -1)
		out = _conn[c].to();
	    else
		out.port = -2;
	}
	c = _conn[c].next_from();
    }
    return out.port >= 0;
}

void
RouterT::find_connections_from(const PortT &h, Vector<PortT> &v) const
{
    assert(h.router() == this);
    int c = _first_conn[h.idx()].from;
    int p = h.port;
    v.clear();
    while (c >= 0) {
	if (_conn[c].from().port == p)
	    v.push_back(_conn[c].to());
	c = _conn[c].next_from();
    }
}

void
RouterT::find_connections_from(const PortT &h, Vector<int> &v) const
{
    assert(h.router() == this);
    int c = _first_conn[h.idx()].from;
    int p = h.port;
    v.clear();
    while (c >= 0) {
	if (_conn[c].from().port == p)
	    v.push_back(c);
	c = _conn[c].next_from();
    }
}

void
RouterT::find_connections_to(const PortT &h, Vector<PortT> &v) const
{
    assert(h.router() == this);
    int c = _first_conn[h.idx()].to;
    int p = h.port;
    v.clear();
    while (c >= 0) {
	if (_conn[c].to().port == p)
	    v.push_back(_conn[c].from());
	c = _conn[c].next_to();
    }
}

void
RouterT::find_connections_to(const PortT &h, Vector<int> &v) const
{
    assert(h.router() == this);
    int c = _first_conn[h.idx()].to;
    int p = h.port;
    v.clear();
    while (c >= 0) {
	if (_conn[c].to().port == p)
	    v.push_back(c);
	c = _conn[c].next_to();
    }
}

void
RouterT::find_connection_vector_from(ElementT *e, Vector<int> &v) const
{
    assert(e->router() == this);
    v.clear();
    int c = _first_conn[e->idx()].from;
    while (c >= 0) {
	int p = _conn[c].from().port;
	if (p >= v.size())
	    v.resize(p + 1, -1);
	if (v[p] >= 0)
	    v[p] = -2;
	else
	    v[p] = c;
	c = _conn[c].next_from();
    }
}

void
RouterT::find_connection_vector_to(ElementT *e, Vector<int> &v) const
{
    assert(e->router() == this);
    v.clear();
    int c = _first_conn[e->idx()].to;
    while (c >= 0) {
	int p = _conn[c].to().port;
	if (p >= v.size())
	    v.resize(p + 1, -1);
	if (v[p] >= 0)
	    v[p] = -2;
	else
	    v[p] = c;
	c = _conn[c].next_to();
    }
}

bool
RouterT::insert_before(const PortT &inserter, const PortT &h)
{
    if (!add_connection(inserter, h))
	return false;

    int i = _first_conn[h.idx()].to;
    while (i >= 0) {
	int next = _conn[i].next_to();
	if (_conn[i].to() == h && connection_live(i)
	    && _conn[i].from() != inserter)
	    change_connection_to(i, inserter);
	i = next;
    }
    return true;
}

bool
RouterT::insert_after(const PortT &inserter, const PortT &h)
{
    if (!add_connection(h, inserter))
	return false;

    int i = _first_conn[h.idx()].from;
    while (i >= 0) {
	int next = _conn[i].next_from();
	if (_conn[i].from() == h && _conn[i].to() != inserter)
	    change_connection_from(i, inserter);
	i = next;
    }
    return true;
}


void
RouterT::add_tunnel(String in, String out, const String &landmark,
		    ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    ElementClassT *tun = ElementClassT::tunnel_type();
    ElementT *ein = get_element(in, tun, String(), landmark);
    ElementT *eout = get_element(out, tun, String(), landmark);

    if (!ein->tunnel())
	errh->lerror(landmark, "redeclaration of element `%s'", in.cc());
    else if (!eout->tunnel())
	errh->lerror(landmark, "redeclaration of element `%s'", out.cc());
    else if (ein->tunnel_output())
	errh->lerror(landmark, "redeclaration of connection tunnel input `%s'", in.cc());
    else if (eout->tunnel_input())
	errh->lerror(landmark, "redeclaration of connection tunnel output `%s'", out.cc());
    else {
	ein->_tunnel_output = eout;
	eout->_tunnel_input = ein;
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
	int trav = _first_conn[i].from;
	int next = 0;		// initialize here to avoid gcc warnings
	while (trav >= 0) {
	    int prev = _first_conn[i].from;
	    int trav_port = _conn[trav].from().port;
	    next = _conn[trav].next_from();
	    while (prev >= 0 && prev != trav) {
		if (_conn[prev].from().port == trav_port
		    && _conn[prev].to() == _conn[trav].to()) {
		    kill_connection(trav);
		    goto duplicate;
		}
		prev = _conn[prev].next_from();
	    }
	  duplicate:
	    trav = next;
	}
    }
}


void
RouterT::remove_dead_elements(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    int nelements = _elements.size();

    // change hookup
    kill_bad_connections();

    // find new element indexes
    Vector<int> new_eindex(nelements, 0);
    int j = 0;
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->dead())
	    new_eindex[i] = -1;
	else
	    new_eindex[i] = j++;
    int new_nelements = j;

    // compress element arrays
    for (int i = 0; i < nelements; i++) {
	j = new_eindex[i];
	if (j < 0) {
	    _element_name_map.insert(_elements[i]->name(), -1);
	    delete _elements[i];
	} else if (j != i) {
	    _element_name_map.insert(_elements[i]->name(), j);
	    _elements[j] = _elements[i];
	    _elements[j]->_idx = j;
	    _first_conn[j] = _first_conn[i];
	}
    }

    // resize element arrays
    _elements.resize(new_nelements);
    _first_conn.resize(new_nelements);
    _real_ecount = new_nelements;
    _free_element = 0;
}

void
RouterT::free_element(ElementT *e)
{
    assert(e->router() == this);
    int ei = e->idx();
    
    // first, remove bad connections from other elements' connection lists
    Vector<int> bad_from, bad_to;
    for (int c = _first_conn[ei].from; c >= 0; c = _conn[c].next_from())
	unlink_connection_to(c);
    for (int c = _first_conn[ei].to; c >= 0; c = _conn[c].next_to())
	unlink_connection_from(c);

    // now, free all of this element's connections
    for (int c = _first_conn[ei].from; c >= 0; ) {
	int next = _conn[c].next_from();
	if (_conn[c].to_idx() != ei)
	    free_connection(c);
	c = next;
    }
    for (int c = _first_conn[ei].to; c >= 0; ) {
	int next = _conn[c].next_to();
	free_connection(c);
	c = next;
    }
    _first_conn[ei] = Pair(-1, -1);

    // finally, free the element itself
    if (_element_name_map[e->name()] == ei)
	_element_name_map.insert(e->name(), -1);
    e->kill();
    e->_tunnel_input = _free_element;
    _free_element = e;
    _real_ecount--;

    check();
}

void
RouterT::free_dead_elements()
{
    int nelements = _elements.size();
    Vector<int> new_eindex(nelements, 0);

    // mark saved findexes
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->dead())
	    new_eindex[i] = -1;

    // don't free elements that have already been freed!!
    for (ElementT *e = _free_element; e; e = e->tunnel_input())
	new_eindex[e->idx()] = 0;

    // get rid of connections to and from dead elements
    kill_bad_connections();

    // free elements
    for (int i = 0; i < nelements; i++)
	if (new_eindex[i] < 0) {
	    ElementT *e = _elements[i];
	    if (_element_name_map[e->name()] == i)
		_element_name_map.insert(e->name(), -1);
	    assert(e->dead());
	    e->_tunnel_input = _free_element;
	    _free_element = e;
	    _real_ecount--;
	}
}


void
RouterT::expand_into(RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
    assert(tor != this);

    int nelements = _elements.size();
    Vector<ElementT *> new_e(nelements, 0);

    // add tunnel pairs and expand below
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->live())
	    new_e[i] = ElementClassT::expand_element(_elements[i], tor, env, errh);

    // add hookup
    int nh = _conn.size();
    for (int i = 0; i < nh; i++) {
	const PortT &hf = _conn[i].from(), &ht = _conn[i].to();
	tor->add_connection(PortT(new_e[hf.idx()], hf.port),
			    PortT(new_e[ht.idx()], ht.port),
			    _conn[i].landmark());
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


static const int PORT_NOT_EXPANDED = -1;
static const int PORT_EXPANDING = -2;

void
RouterT::expand_tunnel(Vector<PortT> *port_expansions,
		       const Vector<PortT> &ports,
		       bool is_output, int which,
		       ErrorHandler *errh) const
{
    Vector<PortT> &expanded = port_expansions[which];

    // quit if circular or already expanded
    if (expanded.size() != 1 || expanded[0].port != PORT_NOT_EXPANDED)
	return;

    // expand if not expanded yet
    expanded[0].port = PORT_EXPANDING;

    const PortT &me = ports[which];
    ElementT *other_elt = (is_output ? me.elt->tunnel_input() : me.elt->tunnel_output());
    
    // find connections from tunnel input
    Vector<PortT> connections;
    if (is_output)
	find_connections_to(PortT(other_elt, me.port), connections);
    else			// or to tunnel output
	find_connections_from(PortT(other_elt, me.port), connections);

    // give good errors for unused or nonexistent compound element ports
    if (!connections.size()) {
	ElementT *in_elt = (is_output ? other_elt : me.elt);
	ElementT *out_elt = (is_output ? me.elt : other_elt);
	String in_name = in_elt->name();
	String out_name = out_elt->name();
	if (in_name + "/input" == out_name) {
	    const char *message = (is_output ? "`%s' input %d unused"
				   : "`%s' has no input %d");
	    errh->lerror(in_elt->landmark(), message, in_name.cc(), me.port);
	} else if (in_name == out_name + "/output") {
	    const char *message = (is_output ? "`%s' has no output %d"
				   : "`%s' output %d unused");
	    errh->lerror(out_elt->landmark(), message, out_name.cc(), me.port);
	} else {
	    errh->lerror(other_elt->landmark(),
			 "tunnel `%s -> %s' %s %d unused",
			 in_name.cc(), out_name.cc(),
			 (is_output ? "input" : "output"), me.port);
	}
    }

    // expand them
    Vector<PortT> store;
    for (int i = 0; i < connections.size(); i++) {
	// if connected to another tunnel, expand that recursively
	if (connections[i].elt->tunnel()) {
	    int x = connections[i].index_in(ports);
	    if (x >= 0) {
		expand_tunnel(port_expansions, ports, is_output, x, errh);
		const Vector<PortT> &v = port_expansions[x];
		if (v.size() > 1 || (v.size() == 1 && v[0].port >= 0))
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
    Vector<PortT> inputs, outputs;
    int nhook = _conn.size();
    for (int i = 0; i < nhook; i++) {
	const ConnectionT &c = _conn[i];
	if (c.dead())
	    continue;
	if (c.from_elt()->tunnel() && c.from_elt()->tunnel_input())
	    (void) c.from().force_index_in(outputs);
	if (c.to_elt()->tunnel() && c.to_elt()->tunnel_output())
	    (void) c.to().force_index_in(inputs);
    }

    // expand tunnels
    int nin = inputs.size(), nout = outputs.size();
    Vector<PortT> *in_expansions = new Vector<PortT>[nin];
    Vector<PortT> *out_expansions = new Vector<PortT>[nout];
    // initialize to placeholders
    for (int i = 0; i < nin; i++)
	in_expansions[i].push_back(PortT(0, PORT_NOT_EXPANDED));
    for (int i = 0; i < nout; i++)
	out_expansions[i].push_back(PortT(0, PORT_NOT_EXPANDED));
    // actually expand
    for (int i = 0; i < nin; i++)
	expand_tunnel(in_expansions, inputs, false, i, errh);
    for (int i = 0; i < nout; i++)
	expand_tunnel(out_expansions, outputs, true, i, errh);

    // get rid of connections to tunnels
    int nelements = _elements.size();
    int old_nhook = _conn.size();
    for (int i = 0; i < old_nhook; i++) {
	const PortT &hf = _conn[i].from(), &ht = _conn[i].to();

	// skip if uninteresting
	if (hf.dead() || !hf.elt->tunnel() || ht.elt->tunnel())
	    continue;
	int idx = hf.index_in(outputs);
	if (idx < 0)
	    continue;

	// add cross product
	// hf, ht are invalidated by adding new connections!
	PortT safe_ht(ht);
	String landmark = _conn[i].landmark(); // must not be reference!
	const Vector<PortT> &v = out_expansions[idx];
	for (int j = 0; j < v.size(); j++)
	    add_connection(v[j], safe_ht, landmark);
    }
    
    // kill elements with tunnel type
    // but don't kill floating tunnels (like input & output)
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->tunnel()
	    && (_elements[i]->tunnel_output() || _elements[i]->tunnel_input()))
	    _elements[i]->kill();

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
	if (_elements[i]->live()) // allow deleted elements
	    ElementClassT::expand_element(_elements[i], this, env, errh);
}

void
RouterT::flatten(ErrorHandler *errh)
{
    check();
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
	if (_elements[i]->tunnel() && _elements[i]->tunnel_output()) {
	    add_line_directive(sa, _elements[i]->landmark());
	    sa << indent << "connectiontunnel " << _elements[i]->name()
	       << " -> " << _elements[i]->tunnel_output()->name() << ";\n";
	}
    if (sa.length() != old_sa_len)
	sa << "\n";

    // print element declarations
    old_sa_len = sa.length();
    for (int i = 0; i < nelements; i++) {
	const ElementT &e = *_elements[i];
	if (e.dead() || e.tunnel())
	    continue; // skip tunnels
	add_line_directive(sa, e.landmark());
	sa << indent << e.name() << " :: " << e.type()->name();
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
    int nc = _conn.size();
    Bitvector used(nc, false);
    for (int c = 0; c < nc; c++)
	if (_conn[c].dead())
	    used[c] = true;

    // prepare hookup chains
    Vector<int> next(nc, -1);
    Bitvector startchain(nc, true);
    for (int c = 0; c < nc; c++) {
	const PortT &ht = _conn[c].to();
	if (ht.port != 0 || used[c])
	    continue;
	int result = -1;
	for (int d = 0; d < nc; d++)
	    if (d != c && _conn[d].from() == ht && !used[d]) {
		result = d;
		if (_conn[d].to().port == 0)
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
	for (int c = 0; c < nc; c++) {
	    const PortT &hf = _conn[c].from();
	    if (used[c] || !startchain[c])
		continue;
	    
	    sa << indent << hf.elt->name();
	    if (hf.port)
		sa << " [" << hf.port << "]";

	    int d = c;
	    while (d >= 0 && !used[d]) {
		if (d == c)
		    sa << " -> ";
		else
		    sa << "\n" << indent << "    -> ";
		const PortT &ht = _conn[d].to();
		if (ht.port)
		    sa << "[" << ht.port << "] ";
		sa << ht.elt->name();
		used[d] = true;
		d = next[d];
	    }

	    sa << ";\n";
	}

	// add new chains to include cycles
	done = true;
	for (int c = 0; c < nc && done; c++)
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


void
RouterT::iterator::step(RouterT *r, int idx)
{
    int n = (r ? r->nelements() : -1);
    while (idx < n && (_e = r->element(idx), _e->dead()))
	idx++;
    if (idx >= n)
	_e = 0;
}

void
RouterT::const_iterator::step(const RouterT *r, int idx)
{
    int n = (r ? r->nelements() : -1);
    while (idx < n && (_e = r->element(idx), _e->dead()))
	idx++;
    if (idx >= n)
	_e = 0;
}

RouterT::type_iterator::type_iterator(RouterT *r, ElementClassT *type)
    : _type(type ? type : ElementClassT::unused_type())
{
    step(r, 0);
}

void
RouterT::type_iterator::step(RouterT *r, int idx)
{
    int n = (r ? r->nelements() : -1);
    while (idx < n && (_e = r->element(idx), _e->type() != _type))
	idx++;
    if (idx >= n)
	_e = 0;
}

#include <click/vector.cc>
