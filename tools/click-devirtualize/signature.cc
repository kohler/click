/*
 * signature.{cc,hh} -- specializer
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include "signature.hh"
#include "processingt.hh"
#include "toolutils.hh"
#include "elementmap.hh"
#include <click/error.hh>
#include <stdio.h>
#include <string.h>

// determine an element's signature

Signatures::Signatures(const RouterT *router)
  : _router(router), _sigid(router->nelements(), 1)
{
}

void
Signatures::create_phase_0(const ProcessingT &pt)
{
  _sigs.clear();
  _sigs.push_back(SignatureNode(-1)); // not special
  Vector<ElementClassT *> sig_eclass;
  sig_eclass.push_back(0);

  int ne = _router->nelements();
  for (int i = 0; i < ne; i++) {
    if (_sigid[i] == SIG_NOT_SPECIAL)
      continue;
    ElementClassT *ec = _router->etype(i);
    for (int j = 0; j < _sigs.size(); j++)
      if (sig_eclass[j] == ec && pt.same_processing(i, _sigs[j]._eid)) {
	_sigid[i] = j;
	goto found_sigid;
      }
    _sigs.push_back(SignatureNode(i));
    sig_eclass.push_back(ec);
    _sigid[i] = _sigs.size() - 1;
   found_sigid: ;
  }
}

void
Signatures::check_port_numbers(int eid, const ProcessingT &pt)
{
  ElementT *e = const_cast<ElementT *>(_router->element(eid));
  int old_sigid = _sigid[eid];
  if (old_sigid == SIG_NOT_SPECIAL)
    return;

  // create new ports array
  Vector<int> new_ports;
  int ni = e->ninputs(), no = e->noutputs();
  for (int i = 0; i < ni; i++)
      if (pt.input_is_pull(eid, i)) {
	  RouterT::conn_iterator it = _router->find_connections_to(PortT(e, i));
	  if (it.is_back())
	      new_ports.push_back(it->from().port);
      }
  for (int i = 0; i < no; i++)
      if (pt.output_is_push(eid, i)) {
	  RouterT::conn_iterator it = _router->find_connections_from(PortT(e, i));
	  if (it.is_back())
	      new_ports.push_back(it->to().port);
      }

  // check for no interesting connections
  if (new_ports.size() == 0)
    return;

  // add new node to list
  if (_sigs[old_sigid]._connections.size() == 0) {
    // set new from old
    _sigs[old_sigid]._connections.swap(new_ports);
    _sigs[old_sigid]._next = -1;
    return;
  }

  // otherwise, search for a match
  int prev = -1, trav = old_sigid;
  while (trav >= 0) {
    if (memcmp(&_sigs[trav]._connections[0], &new_ports[0],
	       new_ports.size() * sizeof(int)) == 0) {
      _sigid[eid] = trav;
      return;
    }
    prev = trav;
    trav = _sigs[trav]._next;
  }

  // if not found, append
  _sigs.push_back(SignatureNode(eid));
  SignatureNode &new_node = _sigs.back();
  new_node._connections.swap(new_ports);
  _sigid[eid] = _sigs[prev]._next = _sigs.size() - 1;
}

bool
Signatures::next_phase(int phase, int eid, Vector<int> &new_sigid,
		       const ProcessingT &pt)
{
  ElementT *e = const_cast<ElementT *>(_router->element(eid));
  int old_sigid = _sigid[eid];
  if (old_sigid == SIG_NOT_SPECIAL
      || _sigs[old_sigid]._connections.size() == 0) {
    new_sigid[eid] = old_sigid;
    return false;
  }

  // create new connections
  Vector<int> new_connections;
  int ni = e->ninputs(), no = e->noutputs();
  for (int i = 0; i < ni; i++)
      if (pt.input_is_pull(eid, i)) {
	  RouterT::conn_iterator it = _router->find_connections_to(PortT(e, i));
	  if (it.is_back())
	      new_connections.push_back(_sigid[it->from().eindex()]);
      }
  for (int i = 0; i < no; i++)
      if (pt.output_is_push(eid, i)) {
	  RouterT::conn_iterator it = _router->find_connections_from(PortT(e, i));
	  if (it.is_back())
	      new_connections.push_back(_sigid[it->to().eindex()]);
      }

  // add new node to list
  if (_sigs[old_sigid]._phase != phase) {
    // set new from old
    _sigs[old_sigid]._phase = phase;
    _sigs[old_sigid]._connections.swap(new_connections);
    _sigs[old_sigid]._next = -1;
    new_sigid[eid] = old_sigid;
    return false;
  }

  // otherwise, search for a match
  int prev = -1, trav = old_sigid;
  while (trav >= 0) {
    if (memcmp(&_sigs[trav]._connections[0], &new_connections[0],
	       new_connections.size() * sizeof(int)) == 0) {
      new_sigid[eid] = trav;
      return false;
    }
    prev = trav;
    trav = _sigs[trav]._next;
  }

  // if not found, append
  _sigs.push_back(SignatureNode(eid));
  SignatureNode &new_node = _sigs.back();
  new_node._phase = phase;
  new_node._connections.swap(new_connections);
  new_sigid[eid] = _sigs[prev]._next = _sigs.size() - 1;
  return true;
}

void
Signatures::print_signature() const
{
  fprintf(stderr, "[");
  for (int i = 0; i < _router->nelements(); i++) {
    fprintf(stderr, (i ? ", %s %d" : "%s %d"), _router->ename(i).c_str(),
	    _sigid[i]);
  }
  fprintf(stderr, "]\n");
}

void
Signatures::specialize_class(const String &eclass_name, bool doit)
{
  for (RouterT::const_type_iterator x = _router->begin_elements(ElementClassT::base_type(eclass_name)); x; x++)
    _sigid[x->eindex()] = (doit ? 1 : SIG_NOT_SPECIAL);
}

void
Signatures::analyze(ElementMap &em)
{
  int ne = _router->nelements();
  ProcessingT pt(const_cast<RouterT *>(_router), &em);

  create_phase_0(pt);

  for (int i = 0; i < ne; i++)
    check_port_numbers(i, pt);

  int phase = 0;
  bool alive = true;
  Vector<int> new_sigid = _sigid;
  while (alive) {
    phase++;
    alive = false;
    for (int i = 0; i < ne; i++)
      alive |= next_phase(phase, i, new_sigid, pt);
    _sigid.swap(new_sigid);
  }
}
