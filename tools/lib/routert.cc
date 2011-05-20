// -*- c-basic-offset: 4 -*-
/*
 * routert.{cc,hh} -- tool definition of router
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 2010 Intel Corporation
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
#include "elementmap.hh"
#include "processingt.hh"
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/variableenv.hh>
#include <stdio.h>

RouterT::RouterT()
    : ElementClassT("<router>"),
      _element_name_map(-1), _free_element(0), _n_live_elements(0),
      _new_eindex_collector(0),
      _connsets(0), _conn_head(0), _conn_tail(&_conn_head), _free_conn(0),
      _declared_type_map(-1),
      _archive_map(-1),
      _declaration_scope(0), _scope_cookie(0),
      _scope(0),
      _ninputs(0), _noutputs(0), _scope_order_error(false),
      _circularity_flag(false), _potential_duplicate_connections(false),
      _overload_type(0)
{
}

RouterT::RouterT(const String &name, const LandmarkT &landmark, RouterT *declaration_scope)
    : ElementClassT(name),
      _element_name_map(-1), _free_element(0), _n_live_elements(0),
      _new_eindex_collector(0),
      _connsets(0), _conn_head(0), _conn_tail(&_conn_head), _free_conn(0),
      _declared_type_map(-1),
      _archive_map(-1),
      _declaration_scope(declaration_scope), _scope_cookie(0),
      _scope(declaration_scope ? &declaration_scope->_scope : 0),
      _ninputs(0), _noutputs(0), _scope_order_error(false),
      _circularity_flag(false), _potential_duplicate_connections(false),
      _overload_type(0), _type_landmark(landmark)
{
    // borrow definitions from 'declaration'
    if (_declaration_scope) {
	_declaration_scope_cookie = _declaration_scope->_scope_cookie;
	_declaration_scope->_scope_cookie++;
	_declaration_scope->use();
    }
    // create input and output pseudoelements
    get_element("input", ElementClassT::tunnel_type(), String(), landmark);
    get_element("output", ElementClassT::tunnel_type(), String(), landmark);
    *(_traits.component(Traits::D_CLASS)) = name;
}

RouterT::~RouterT()
{
    for (int i = 0; i < _elements.size(); i++)
	delete _elements[i];
    while (ConnectionSet *cs = _connsets) {
	_connsets = cs->next;
	delete[] reinterpret_cast<char *>(cs);
    }
    if (_overload_type)
	_overload_type->unuse();
    if (_declaration_scope)
	_declaration_scope->unuse();
}

void
RouterT::check() const
{
    int ne = nelements();
    int nt = _declared_types.size();

    // check basic sizes
    assert(_elements.size() == _first_conn.size());

    // check element type names
    int nt_found = 0;
    for (StringMap::const_iterator iter = _declared_type_map.begin(); iter.live(); iter++) {
	int sc = _scope_cookie;
	for (int i = iter.value(); i >= 0; i = _declared_types[i].prev_name) {
	    assert(_declared_types[i].name() == iter.key());
	    assert(_declared_types[i].prev_name < i);
	    assert(_declared_types[i].scope_cookie <= sc);
	    sc = _declared_types[i].scope_cookie - 1;
	    nt_found++;
	}
    }
    // note that nt_found might not equal nt, because of anonymous classes

    // check element types
    HashTable<ElementClassT *, int> type_map(-1);
    for (int i = 0; i < nt; i++) {
	assert(type_map[_declared_types[i].type] < 0);
	type_map.set(_declared_types[i].type, i);
    }

    // check element names
    for (StringMap::const_iterator iter = _element_name_map.begin(); iter.live(); iter++) {
	String key = iter.key();
	int value = iter.value();
	if (value >= 0)
	    assert(value < ne && _elements[value]->name() == key); // && _elements[value].live());
    }

    // check free elements
    for (ElementT *e = _free_element; e; e = e->tunnel_input()) {
	assert(e->dead());
	assert(_elements[e->eindex()] == e);
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
    for (ConnectionX *c = _conn_head; c; c = c->_next[end_all]) {
	assert(c->live());
	ConnectionX *d, *e;
	for (d = _first_conn[c->from_eindex()][end_from]; d != c; d = d->_next[end_from])
	    /* nada */;
	for (e = _first_conn[c->to_eindex()][end_to]; e != c; e = e->_next[end_to])
	    /* nada */;
	assert(d && e);
    }

    // check hookup next pointers, port counts
    for (int i = 0; i < ne; i++)
	if (_elements[i]->live()) {
	    int ninputs = 0, noutputs = 0;
	    const ElementT *e = element(i);

	    for (ConnectionX *c = _first_conn[i][end_from]; c; c = c->next_from()) {
		assert(c->from_element() == e);
		if (c->from().port >= noutputs)
		    noutputs = c->from().port + 1;
		if (!_potential_duplicate_connections)
		    for (ConnectionX *d = c->next_from(); d; d = d->next_from())
			assert(d->from_port() != c->from_port()
			       || d->to() != c->to());
	    }

	    for (ConnectionX *c = _first_conn[i][end_to]; c; c = c->next_to()) {
		assert(c->to_element() == e && c->live());
		if (c->to().port >= ninputs)
		    ninputs = c->to().port + 1;
	    }

	    assert(ninputs == e->ninputs() && noutputs == e->noutputs());
	}
}


ElementT *
RouterT::add_element(const ElementT &elt_in)
{
    int i;
    _n_live_elements++;
    ElementT *elt = new ElementT(elt_in);
    if (_free_element) {
	i = _free_element->eindex();
	_free_element = _free_element->tunnel_input();
	delete _elements[i];
	_first_conn[i] = Pair(0, 0);
    } else {
	i = _elements.size();
	_elements.push_back(0);
	_first_conn.push_back(Pair(0, 0));
    }
    if (_new_eindex_collector)
	_new_eindex_collector->push_back(i);
    _elements[i] = elt;
    elt->_eindex = i;
    elt->_owner = this;
    return elt;
}

ElementT *
RouterT::get_element(const String &name, ElementClassT *type,
		     const String &config, const LandmarkT &landmark)
{
    int &i = _element_name_map[name];
    if (i >= 0)
	return _elements[i];
    else {
	ElementT *e = add_element(ElementT(name, type, config, landmark));
	i = e->eindex();
	return e;
    }
}

ElementT *
RouterT::add_anon_element(ElementClassT *type, const String &config,
			  const LandmarkT &landmark)
{
    String name = ";" + type->name() + "@" + String(_n_live_elements + 1);
    ElementT *result = add_element(ElementT(name, type, config, landmark));
    assign_element_name(result->eindex());
    return result;
}

void
RouterT::change_ename(int ei, const String &new_name)
{
    assert(ElementT::name_ok(new_name));
    ElementT &e = *_elements[ei];
    if (e.live()) {
	if (eindex(e.name()) == ei)
	    _element_name_map.set(e.name(), -1);
	e._name = new_name;
	_element_name_map.set(new_name, ei);
    }
}

int
RouterT::__map_element_name(const String &name, int new_eindex)
{
    assert(ElementT::name_ok(name));
    int &slot = _element_name_map[name];
    int old_eindex = slot;
    slot = new_eindex;
    return old_eindex;
}

void
RouterT::assign_element_name(int ei)
{
    assert(_elements[ei]->name_unassigned());
    String name = _elements[ei]->name().substring(1);
    if (eindex(name) >= 0) {
	int at_pos = name.find_right('@');
	assert(at_pos >= 0);
	String prefix = name.substring(0, at_pos + 1);
	const char *abegin = name.begin() + at_pos + 1, *aend = abegin;
	while (aend < name.end() && isdigit((unsigned char) *aend))
	    ++aend;
	int anonymizer;
	cp_integer(abegin, aend, 10, &anonymizer);
	do {
	    anonymizer++;
	    name = prefix + String(anonymizer);
	} while (eindex(name) >= 0);
    }
    change_ename(ei, name);
}

void
RouterT::assign_element_names()
{
    for (int i = 0; i < _elements.size(); i++)
	if (_elements[i]->name_unassigned())
	    assign_element_name(i);
}


//
// TYPES
//

ElementClassT *
RouterT::locally_declared_type(const String &name) const
{
    int i = _declared_type_map[name];
    return (i >= 0 ? _declared_types[i].type : 0);
}

ElementClassT *
RouterT::declared_type(const String &name, int scope_cookie) const
{
    for (const RouterT *r = this; r; scope_cookie = r->_declaration_scope_cookie, r = r->_declaration_scope)
	for (int i = r->_declared_type_map[name]; i >= 0; i = r->_declared_types[i].prev_name)
	    if (r->_declared_types[i].scope_cookie <= scope_cookie)
		return r->_declared_types[i].type;
    return 0;
}

void
RouterT::add_declared_type(ElementClassT *ec, bool anonymous)
{
    assert(ec);
    if (anonymous || !ec->name())
	_declared_types.push_back(ElementType(ec, _scope_cookie, -1));
    else if (locally_declared_type(ec->name()) != ec) {
	int prev = _declared_type_map[ec->name()];
	if (prev >= 0)		// increment scope_cookie if redefining class
	    _scope_cookie++;
	_declared_types.push_back(ElementType(ec, _scope_cookie, prev));
	_declared_type_map.set(ec->name(), _declared_types.size() - 1);
    }
}

void
RouterT::collect_types(HashTable<ElementClassT *, int> &m) const
{
    HashTable<ElementClassT *, int>::iterator it = m.find_insert(const_cast<RouterT *>(this), 0);
    if (it.value() == 0) {
	it.value() = 1;
	for (int i = 0; i < _declared_types.size(); i++)
	    _declared_types[i].type->collect_types(m);
	for (Vector<ElementT *>::const_iterator it = _elements.begin();
	     it != _elements.end(); ++it)
	    if ((*it)->live())
		(*it)->type()->collect_types(m);
	if (_overload_type)
	    _overload_type->collect_types(m);
    }
}

void
RouterT::collect_locally_declared_types(Vector<ElementClassT *> &v) const
{
    for (Vector<ElementType>::const_iterator i = _declared_types.begin(); i != _declared_types.end(); i++)
	v.push_back(i->type);
}

void
RouterT::collect_overloads(Vector<ElementClassT *> &v) const
{
    if (_overload_type)
	_overload_type->collect_overloads(v);
    v.push_back(const_cast<RouterT *>(this));
}


//
// CONNECTIONS
//

bool
RouterT::add_connection(const PortT &hfrom, const PortT &hto,
			const LandmarkT &landmark)
{
    assert(hfrom.router() == this && hfrom.element->live()
	   && hto.router() == this && hto.element->live());

    Pair &first_from = _first_conn[hfrom.eindex()];
    Pair &first_to = _first_conn[hto.eindex()];

    // maintain port counts (or ignore duplicate connections)
    if (hfrom.port < hfrom.element->noutputs() && hto.port < hto.element->ninputs()) {
	for (ConnectionX *cx = first_from[end_from]; cx; cx = cx->next_from())
	    if (cx->from_port() == hfrom.port && cx->to() == hto)
		return true;
    } else {
	if (hfrom.port >= hfrom.element->noutputs())
	    hfrom.element->set_noutputs(hfrom.port + 1);
	if (hto.port >= hto.element->ninputs())
	    hto.element->set_ninputs(hto.port + 1);
    }

    if (!_free_conn) {
	int n = (_connsets ? 2047 : 31);
	ConnectionSet *cs = reinterpret_cast<ConnectionSet *>(new char[sizeof(ConnectionSet) + (n - 1) * sizeof(ConnectionX)]);
	if (!cs)
	    return false;
	cs->next = _connsets;
	for (int i = 0; i < n; ++i) {
	    new((void *) &cs->c[i]) ConnectionT;
	    cs->c[i]._next[0] = (i + 1 < n ? &cs->c[i+1] : 0);
	}
	_connsets = cs;
	_free_conn = &cs->c[0];
    }

    ConnectionX *c = _free_conn;
    _free_conn = c->_next[0];
    *c = ConnectionX(hfrom, hto, landmark);

    c->_pprev = _conn_tail;
    c->_next[end_all] = 0;
    *_conn_tail = c;
    _conn_tail = &c->_next[end_all];

    c->_next[end_from] = first_from[end_from];
    c->_next[end_to] = first_to[end_to];
    first_from[end_from] = first_to[end_to] = c;

    return true;
}

void
RouterT::update_noutputs(int e)
{
    int n = 0;
    for (ConnectionX *cx = _first_conn[e][end_from]; cx; cx = cx->next_from())
	if (cx->from().port >= n)
	    n = cx->from().port + 1;
    _elements[e]->set_noutputs(n);
}

void
RouterT::update_ninputs(int e)
{
    int n = 0;
    for (ConnectionX *cx = _first_conn[e][end_to]; cx; cx = cx->next_to())
	if (cx->to().port >= n)
	    n = cx->to().port + 1;
    _elements[e]->set_ninputs(n);
}

void
RouterT::unlink_connection_from(ConnectionX *c)
{
    int e = c->from_eindex();
    int port = c->from_port();

    // find previous connection
    ConnectionX **pprev = &_first_conn[e][end_from];
    while (*pprev && *pprev != c)
	pprev = &(*pprev)->_next[end_from];
    // unlink this connection
    assert(*pprev == c);
    *pprev = c->next_from();

    // update port count
    if (_elements[e]->_noutputs == port + 1)
	update_noutputs(e);
}

void
RouterT::unlink_connection_to(ConnectionX *c)
{
    int e = c->to_eindex();
    int port = c->to_port();

    // find previous connection
    ConnectionX **pprev = &_first_conn[e][end_to];
    while (*pprev && *pprev != c)
	pprev = &(*pprev)->_next[end_to];
    // unlink this connection
    assert(*pprev == c);
    *pprev = c->next_to();

    // update port count
    if (_elements[e]->_ninputs == port + 1)
	update_ninputs(e);
}

void
RouterT::free_connection(ConnectionX *c)
{
    *c->_pprev = c->_next[end_all];
    if (c->_next[end_all])
	c->_next[end_all]->_pprev = c->_pprev;
    if (_conn_tail == &c->_next[end_all])
	_conn_tail = c->_pprev;
    c->_next[0] = _free_conn;
    _free_conn = c;
}

RouterT::conn_iterator
RouterT::erase(conn_iterator ci)
{
    if (ci) {
	ConnectionX *c = const_cast<ConnectionX *>(ci._conn);
	++ci;
	assert(c->router() == this);
	unlink_connection_from(c);
	unlink_connection_to(c);
	free_connection(c);
    }
    return ci;
}

void
RouterT::kill_bad_connections()
{
    for (conn_iterator ci = begin_connections(); ci != end_connections(); ++ci)
	if (ci->from_element()->dead() || ci->to_element()->dead())
	    erase(ci);
}

RouterT::conn_iterator
RouterT::change_connection_from(conn_iterator it, PortT h)
{
    assert(h.router() == this && it->router() == this);
    ConnectionX *c = const_cast<ConnectionX *>(it._conn);
    ++it;			// increment now to precede modifications

    unlink_connection_from(c);

    c->_end[end_from] = h;
    if (h.port >= h.element->_noutputs)
	h.element->_noutputs = h.port + 1;

    int ei = h.eindex();
    c->_next[end_from] = _first_conn[ei][end_from];
    _first_conn[ei][end_from] = c;
    _potential_duplicate_connections = true;

    return it;
}

RouterT::conn_iterator
RouterT::change_connection_to(conn_iterator it, PortT h)
{
    assert(h.router() == this && it->router() == this);
    ConnectionX *c = const_cast<ConnectionX *>(it._conn);
    ++it;			// increment now to precede modifications

    unlink_connection_to(c);

    c->_end[end_to] = h;
    if (h.port >= h.element->ninputs())
	h.element->set_ninputs(h.port + 1);

    int ei = h.eindex();
    c->_next[end_to] = _first_conn[ei][end_to];
    _first_conn[ei][end_to] = c;
    _potential_duplicate_connections = true;

    return it;
}

RouterT::conn_iterator
RouterT::find_connections_touching(int eindex, int port, bool isoutput) const
{
    assert(eindex >= 0 && eindex < nelements());
    ConnectionX *c = _first_conn[eindex][isoutput];
    unsigned by;
    if (port >= 0) {
	while (c && c->port(isoutput) != port)
	    c = c->_next[isoutput];
	by = (isoutput ? port + 3 : -port - 3);
    } else
	by = isoutput;
    return conn_iterator(c, by);
}

void
RouterT::conn_iterator::complex_step()
{
    if (unsigned(_by) <= end_all)
	_conn = _conn->_next[_by];
    else {
	bool isoutput = _by > 0;
	int port = (isoutput ? _by - 3 : -_by - 3);
	do {
	    _conn = _conn->_next[isoutput];
	} while (_conn && _conn->port(isoutput) != port);
    }
}

bool
RouterT::has_connection(const PortT &hfrom, const PortT &hto) const
{
    assert(hfrom.router() == this && hto.router() == this);
    ConnectionX *c = _first_conn[hfrom.eindex()][end_from];
    while (c && (c->from() != hfrom || c->to() != hto))
	c = c->next_from();
    return c;
}

void
RouterT::find_connections_touching(const PortT &port, bool isoutput, Vector<PortT> &v, bool clear) const
{
    assert(port.router() == this);
    ConnectionX *c = _first_conn[port.eindex()][isoutput];
    int p = port.port;
    if (clear)
	v.clear();
    while (c) {
	if (c->port(isoutput) == p)
	    v.push_back(c->end(!isoutput));
	c = c->_next[isoutput];
    }
}

void
RouterT::find_connection_vector_touching(const ElementT *e, bool isoutput, Vector<conn_iterator> &x) const
{
    assert(e->router() == this);
    x.clear();
    ConnectionX *c = _first_conn[e->eindex()][isoutput];
    while (c) {
	int p = c->port(isoutput);
	if (p >= x.size())
	    x.resize(p + 1);
	if (!x[p])
	    x[p].assign(c, isoutput ? p + 3 : -p - 3);
	c = c->_next[isoutput];
    }
}

bool
RouterT::insert_before(const PortT &inserter, const PortT &h)
{
    if (!add_connection(inserter, h))
	return false;

    ConnectionX *c = _first_conn[h.eindex()][end_to];
    while (c) {
	ConnectionX *next = c->next_to();
	if (c->to() == h && c->from() != inserter)
	    change_connection_to(conn_iterator(c, 0), inserter);
	c = next;
    }
    return true;
}

bool
RouterT::insert_after(const PortT &inserter, const PortT &h)
{
    if (!add_connection(h, inserter))
	return false;

    ConnectionX *c = _first_conn[h.eindex()][end_from];
    while (c) {
	ConnectionX *next = c->next_from();
	if (c->from() == h && c->to() != inserter)
	    change_connection_from(conn_iterator(c, 0), inserter);
	c = next;
    }
    return true;
}


void
RouterT::add_tunnel(const String &namein, const String &nameout,
		    const LandmarkT &landmark, ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    ElementClassT *tun = ElementClassT::tunnel_type();
    ElementT *ein = get_element(namein, tun, String(), landmark);
    ElementT *eout = get_element(nameout, tun, String(), landmark);

    bool ok = true;
    if (!ein->tunnel()) {
	ElementT::redeclaration_error(errh, "element", namein, landmark, ein->landmark());
	ok = false;
    }
    if (!eout->tunnel()) {
	ElementT::redeclaration_error(errh, "element", nameout, landmark, eout->landmark());
	ok = false;
    }
    if (ein->tunnel_output()) {
	ElementT::redeclaration_error(errh, "connection tunnel input", namein, landmark, ein->landmark());
	ok = false;
    }
    if (eout->tunnel_input()) {
	ElementT::redeclaration_error(errh, "connection tunnel output", nameout, landmark, eout->landmark());
	ok = false;
    }
    if (ok) {
	ein->_tunnel_output = eout;
	eout->_tunnel_input = ein;
    }
}


//
// REQUIREMENTS
//

void
RouterT::add_requirement(const String &type, const String &value)
{
    _requirements.push_back(type);
    _requirements.push_back(value);
}

void
RouterT::remove_requirement(const String &type, const String &value)
{
    for (int i = 0; i < _requirements.size(); i += 2)
	if (_requirements[i] == type && _requirements[i+1] == value) {
	    // keep requirements in order
	    for (int j = i + 2; j < _requirements.size(); j++)
		_requirements[j-2] = _requirements[j];
	    _requirements.resize(_requirements.size() - 2);
	    return;
	}
}


void
RouterT::add_archive(const ArchiveElement &ae)
{
    int &i = _archive_map[ae.name];
    if (i >= 0)
	_archive[i] = ae;
    else {
	i = _archive.size();
	_archive.push_back(ae);
    }
}


void
RouterT::remove_duplicate_connections()
{
    if (!_potential_duplicate_connections)
	return;

    // 5.Dec.1999 - This function dominated the running time of click-xform.
    // Use an algorithm faster on the common case (few connections per
    // element).

    int nelem = _elements.size();
    Vector<int> removers;

    for (int i = 0; i < nelem; i++)
	if (ConnectionX *first = _first_conn[i][end_from]) {
	    ConnectionX *trav = first->_next[end_from];
	    while (trav) {
		ConnectionX *dup = first, *next = trav->_next[end_from];
		while (dup != trav && (dup->from().port != trav->from().port
				       || dup->to() != trav->to()))
		    dup = dup->_next[end_from];
		if (dup != trav)
		    erase(conn_iterator(trav, 0));
		trav = next;
	    }
	}

    _potential_duplicate_connections = false;
}

void
RouterT::remove_dead_elements()
{
    int nelements = _elements.size();

    // find new element indexes
    Vector<int> new_eindex(nelements, 0);
    int j = 0;
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->dead())
	    new_eindex[i] = -1;
	else
	    new_eindex[i] = j++;
    int new_nelements = j;
    if (new_nelements == nelements)
	return;

    // change hookup
    kill_bad_connections();

    // compress element arrays
    for (int i = 0; i < nelements; i++) {
	j = new_eindex[i];
	if (j < 0) {
	    _element_name_map.set(_elements[i]->name(), -1);
	    delete _elements[i];
	} else if (j != i) {
	    _element_name_map.set(_elements[i]->name(), j);
	    _elements[j] = _elements[i];
	    _elements[j]->_eindex = j;
	    _first_conn[j] = _first_conn[i];
	}
    }

    // resize element arrays
    _elements.resize(new_nelements);
    _first_conn.resize(new_nelements);
    _n_live_elements = new_nelements;
    _free_element = 0;
}

void
RouterT::free_element(ElementT *e)
{
    assert(e->router() == this);
    int ei = e->eindex();

    // first, remove bad connections from other elements' connection lists
    for (ConnectionX *c = _first_conn[ei][end_from]; c; c = c->next_from())
	unlink_connection_to(c);
    for (ConnectionX *c = _first_conn[ei][end_to]; c; c = c->next_to())
	unlink_connection_from(c);

    // now, free all of this element's connections
    while (ConnectionX *c = _first_conn[ei][end_from]) {
	_first_conn[ei][end_from] = c->_next[end_from];
	if (c->to_eindex() != ei)
	    free_connection(c);
    }
    while (ConnectionX *c = _first_conn[ei][end_to]) {
	_first_conn[ei][end_to] = c->_next[end_to];
	free_connection(c);
    }
    _first_conn[ei] = Pair(0, 0);

    // finally, free the element itself
    if (_element_name_map[e->name()] == ei)
	_element_name_map.set(e->name(), -1);
    e->kill();
    e->_tunnel_input = _free_element;
    _free_element = e;
    _n_live_elements--;

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
	new_eindex[e->eindex()] = 0;

    // get rid of connections to and from dead elements
    kill_bad_connections();

    // free elements
    for (int i = 0; i < nelements; i++)
	if (new_eindex[i] < 0) {
	    ElementT *e = _elements[i];
	    if (_element_name_map[e->name()] == i)
		_element_name_map.set(e->name(), -1);
	    assert(e->dead());
	    e->_tunnel_input = _free_element;
	    _free_element = e;
	    _n_live_elements--;
	}
}


void
RouterT::expand_into(RouterT *tor, const String &prefix, VariableEnvironment &env, ErrorHandler *errh)
{
    assert(tor != this);
    assert(!prefix || prefix.back() == '/');

    int nelements = _elements.size();
    Vector<ElementT *> new_e(nelements, 0);

    // add tunnel pairs and expand below
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->live())
	    new_e[i] = ElementClassT::expand_element(_elements[i], tor, prefix, env, errh);

    // add hookup
    for (ConnectionX *c = _conn_head; c; c = c->_next[end_all])
	tor->add_connection(PortT(new_e[c->from_eindex()], c->from_port()),
			    PortT(new_e[c->to_eindex()], c->to_port()),
			    c->landmarkt());

    // add requirements
    for (int i = 0; i < _requirements.size(); i += 2)
	tor->add_requirement(_requirements[i], _requirements[i+1]);

    // add archive elements
    for (int i = 0; i < _archive.size(); i++) {
	const ArchiveElement &ae = _archive[i];
	if (ae.live() && ae.name != "config") {
	    if (tor->archive_index(ae.name) >= 0)
		errh->error("expansion confict: two archive elements named %<%s%>", ae.name.c_str());
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
    ElementT *other_elt = (is_output ? me.element->tunnel_input() : me.element->tunnel_output());

    // find connections from tunnel input or to tunnel output
    Vector<PortT> connections;
    find_connections_touching(PortT(other_elt, me.port), !is_output, connections);

    // give good errors for unused or nonexistent compound element ports
    if (!connections.size()) {
	ElementT *in_elt = (is_output ? other_elt : me.element);
	ElementT *out_elt = (is_output ? me.element : other_elt);
	String in_name = in_elt->name();
	String out_name = out_elt->name();
	if (in_name + "/input" == out_name) {
	    const char *message = (is_output ? "%<%s%> input %d unused"
				   : "%<%s%> has no input %d");
	    errh->lerror(in_elt->landmark(), message, in_name.c_str(), me.port);
	} else if (in_name == out_name + "/output") {
	    const char *message = (is_output ? "%<%s%> has no output %d"
				   : "%<%s%> output %d unused");
	    errh->lerror(out_elt->landmark(), message, out_name.c_str(), me.port);
	} else {
	    errh->lerror(other_elt->landmark(),
			 "tunnel %<%s -> %s%> %s %d unused",
			 in_name.c_str(), out_name.c_str(),
			 (is_output ? "input" : "output"), me.port);
	}
    }

    // expand them
    Vector<PortT> store;
    for (int i = 0; i < connections.size(); i++) {
	// if connected to another tunnel, expand that recursively
	if (connections[i].element->tunnel()) {
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
    for (ConnectionX *c = _conn_head; c; c = c->_next[end_all]) {
	if (c->from_element()->tunnel() && c->from_element()->tunnel_input())
	    (void) c->from().force_index_in(outputs);
	if (c->to_element()->tunnel() && c->to_element()->tunnel_output())
	    (void) c->to().force_index_in(inputs);
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
    ConnectionX **tail = _conn_tail;
    for (ConnectionX *c = _conn_head; c != *tail; c = c->_next[end_all]) {
	// skip if uninteresting
	if (!c->from_element()->tunnel() || c->to_element()->tunnel())
	    continue;
	int x = c->from().index_in(outputs);
	if (x < 0)
	    continue;

	// add cross product
	const Vector<PortT> &v = out_expansions[x];
	for (int j = 0; j < v.size(); j++)
	    add_connection(v[j], c->to(), c->landmarkt());
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

    delete[] in_expansions;
    delete[] out_expansions;
}

void
RouterT::compact()
{
    remove_dead_elements();
}


void
RouterT::remove_compound_elements(ErrorHandler *errh, bool expand_vars)
{
    int nelements = _elements.size();

    // construct a fake VariableEnvironment so we preserve variable names
    // even in the presence of ${NAME-DEFAULT}
    VariableEnvironment ve(0);
    for (int i = 0; i < _scope.size(); i++)
	if (expand_vars)
	    ve.define(_scope.name(i), cp_expand(_scope.value(i), ve), true);
	else
	    ve.define(_scope.name(i), String("$") + _scope.name(i), true);

    for (int i = 0; i < nelements; i++)
	if (_elements[i]->live()) // allow deleted elements
	    ElementClassT::expand_element(_elements[i], this, String(), ve, errh);
}

void
RouterT::flatten(ErrorHandler *errh, bool expand_vars)
{
    check();
    //String s = configuration_string(); fprintf(stderr, "1.\n%s\n\n", s.c_str());
    remove_compound_elements(errh, expand_vars);
    //s = configuration_string(); fprintf(stderr, "2.\n%s\n\n", s.c_str());
    remove_tunnels(errh);
    compact();
    //s = configuration_string(); fprintf(stderr, "5.\n%s\n\n", s.c_str());
    _declared_type_map.clear();
    _declared_types.clear();
    if (expand_vars)
	_scope.clear();
    check();
}

void
RouterT::const_iterator::step(const RouterT *r, int eindex)
{
    int n = (r ? r->nelements() : -1);
    while (eindex < n && (_e = r->element(eindex), _e->dead()))
	eindex++;
    if (eindex >= n)
	_e = 0;
}

void
RouterT::const_type_iterator::step(const RouterT *r, int eindex)
{
    int n = (r ? r->nelements() : -1);
    while (eindex < n && (_e = r->element(eindex), _e->type() != _t))
	eindex++;
    if (eindex >= n)
	_e = 0;
}


//
// TYPE METHODS
//

const ElementTraits *
RouterT::find_traits(ElementMap *emap) const
{
    // Do not resolve agnostics to push, or the flow code will be wrong.
    SilentErrorHandler serrh;
    ProcessingT pt(false, const_cast<RouterT *>(this), emap, &serrh);
    *(_traits.component(Traits::D_PORT_COUNT)) = pt.compound_port_count_code();
    *(_traits.component(Traits::D_PROCESSING)) = pt.compound_processing_code();
    *(_traits.component(Traits::D_FLOW_CODE)) = pt.compound_flow_code();
    return &_traits;
}

int
RouterT::check_pseudoelement(const ElementT *e, bool isoutput, const char *name, ErrorHandler *errh) const
{
    static const char * const names[2] = {"input", "output"};
    Vector<conn_iterator> used;
    find_connection_vector_touching(e, !isoutput, used);
    if (e->nports(isoutput))
	errh->error("%<%s%> pseudoelement %<%s%> may only be used as %s", name, names[isoutput], names[1-isoutput]);
    for (int i = 0; i < used.size(); ++i)
	if (!used[i])
	    errh->error("%<%s%> %s %d missing", name, names[isoutput], i);
    return e->nports(!isoutput);
}

int
RouterT::finish_type(ErrorHandler *errh)
{
    LocalErrorHandler lerrh(errh);
    LandmarkErrorHandler landerrh(&lerrh, _type_landmark.str());

    if (ElementT *einput = element("input"))
	_ninputs = check_pseudoelement(einput, false, printable_name_c_str(), &landerrh);
    else
	_ninputs = 0;

    if (ElementT *eoutput = element("output"))
	_noutputs = check_pseudoelement(eoutput, true, printable_name_c_str(), &landerrh);
    else
	_noutputs = 0;

    // resolve anonymous element names
    assign_element_names();

    return (lerrh.nerrors() ? -1 : 0);
}

void
RouterT::set_overload_type(ElementClassT *t)
{
    assert(!_overload_type);
    _overload_type = t;
    if (_overload_type)
	_overload_type->use();
}

inline int
RouterT::assign_arguments(const Vector<String> &args, Vector<String> *values) const
{
    return cp_assign_arguments(args, _formal_types.begin(), _formal_types.end(), values);
}

bool
RouterT::need_resolve() const
{
    return true;		// always resolve compound b/c of arguments
}

bool
RouterT::overloaded() const
{
    return _overload_type != 0;
}

ElementClassT *
RouterT::resolve(int ninputs, int noutputs, Vector<String> &args, ErrorHandler *errh, const LandmarkT &landmark)
{
    // Try to return an element class, even if it is wrong -- the error
    // messages are friendlier
    RouterT *r = this;
    RouterT *closest = 0;
    int nct = 0;

    while (1) {
	nct++;
	if (r->_ninputs == ninputs && r->_noutputs == noutputs
	    && r->assign_arguments(args, &args) >= 0)
	    return r;
	else if (r->assign_arguments(args, 0) >= 0)
	    closest = r;

	ElementClassT *overload = r->_overload_type;
	if (!overload)
	    break;
	else if (RouterT *next = overload->cast_router())
	    r = next;
	else if (ElementClassT *result = overload->resolve(ninputs, noutputs, args, errh, landmark))
	    return result;
	else
	    break;
    }

    if (nct != 1 || !closest) {
	errh->lerror(landmark.decorated_str(), "no match for %<%s%>", ElementClassT::unparse_signature(name(), 0, args.size(), ninputs, noutputs).c_str());
	ContextErrorHandler cerrh(errh, "candidates are:");
	for (r = this; r; r = (r->_overload_type ? r->_overload_type->cast_router() : 0))
	    cerrh.lmessage(r->decorated_landmark(), "%s", r->unparse_signature().c_str());
    }
    if (closest)
	closest->assign_arguments(args, &args);
    return closest;
}

void
RouterT::create_scope(const Vector<String> &args,
		      const VariableEnvironment &env,
		      VariableEnvironment &new_env)
{
    assert(&new_env != &env);
    new_env = VariableEnvironment(env.parent_of(_scope.depth()));
    for (int i = 0; i < _formals.size() && i < args.size(); i++)
	new_env.define(_formals[i], args[i], true);
    for (int i = args.size(); i < _formals.size(); i++)
	new_env.define(_formals[i], String(), true);
    for (int i = 0; i < _scope.size(); i++)
	new_env.define(_scope.name(i), cp_expand(_scope.value(i), env), true);
}

ElementT *
RouterT::complex_expand_element(
	ElementT *compound, const Vector<String> &args,
	RouterT *tor, const String &prefix,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    RouterT *fromr = compound->router();
    assert(fromr != this && tor != this);
    assert(!_circularity_flag);
    // ensure we don't delete ourselves before we're done!
    use();
    _circularity_flag = true;

    // parse configuration string
    int nargs = _formals.size();
    if (args.size() != nargs) {
	const char *whoops = (args.size() < nargs ? "few" : "many");
	String signature;
	for (int i = 0; i < nargs; i++) {
	    if (i) signature += ", ";
	    signature += _scope.name(i);
	}
	if (errh)
	    errh->lerror(compound->landmark(),
			 "too %s arguments to compound element %<%s(%s)%>",
			 whoops, printable_name_c_str(), signature.c_str());
    }

    // create prefix
    assert(compound->name());
    VariableEnvironment new_env(0);
    create_scope(args, env, new_env);
    String new_prefix = prefix + compound->name(); // includes previous prefix
    if (new_prefix.back() != '/')
	new_prefix += '/';

    // create input/output tunnels
    if (fromr == tor)
	compound->set_type(tunnel_type());
    tor->add_tunnel(prefix + compound->name(), new_prefix + "input", compound->landmarkt(), errh);
    tor->add_tunnel(new_prefix + "output", prefix + compound->name(), compound->landmarkt(), errh);
    ElementT *new_e = tor->element(prefix + compound->name());

    // dump compound router into 'tor'
    expand_into(tor, new_prefix, new_env, errh);

    // yes, we expanded it
    _circularity_flag = false;
    unuse();
    return new_e;
}

String
RouterT::unparse_signature() const
{
    return ElementClassT::unparse_signature(name(), &_scope.values(), -1, ninputs(), noutputs());
}
