/*
 * signature.{cc,hh} -- specializer
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "signature.hh"
#include "processingt.hh"
#include "toolutils.hh"
#include "error.hh"
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
  Vector<int> sig_eclass;
  sig_eclass.push_back(0);

  int ne = _router->nelements();
  for (int i = 0; i < ne; i++) {
    if (_sigid[i] == SIG_NOT_SPECIAL)
      continue;
    int ec = _router->etype(i);
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
  int old_sigid = _sigid[eid];
  if (old_sigid == SIG_NOT_SPECIAL)
    return;

  // create new ports array
  Vector<int> new_ports;
  int ni = pt.ninputs(eid), no = pt.noutputs(eid);
  for (int i = 0; i < ni; i++) {
    Hookup h = pt.input_connection(eid, i);
    if (h.idx >= 0)
      new_ports.push_back(h.port);
  }
  for (int i = 0; i < no; i++) {
    Hookup h = pt.output_connection(eid, i);
    if (h.idx >= 0)
      new_ports.push_back(h.port);
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
  int old_sigid = _sigid[eid];
  if (old_sigid == SIG_NOT_SPECIAL
      || _sigs[old_sigid]._connections.size() == 0) {
    new_sigid[eid] = old_sigid;
    return false;
  }

  // create new connections
  Vector<int> new_connections;
  int ni = pt.ninputs(eid), no = pt.noutputs(eid);
  for (int i = 0; i < ni; i++) {
    Hookup h = pt.input_connection(eid, i);
    if (h.idx >= 0)
      new_connections.push_back(_sigid[h.idx]);
  }
  for (int i = 0; i < no; i++) {
    Hookup h = pt.output_connection(eid, i);
    if (h.idx >= 0)
      new_connections.push_back(_sigid[h.idx]);
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
    fprintf(stderr, (i ? ", %s %d" : "%s %d"), _router->ename(i).cc(),
	    _sigid[i]);
  }
  fprintf(stderr, "]\n");
}

void
Signatures::specialize_class(const String &eclass_name, bool doit)
{
  int ec = _router->type_index(eclass_name);
  for (int i = 0; i < _router->nelements(); i++)
    if (_router->etype(i) == ec)
      _sigid[i] = (doit ? 1 : SIG_NOT_SPECIAL);
}

void
Signatures::analyze(const ElementMap &em)
{
  int ne = _router->nelements();
  ProcessingT pt(_router, em, 0);
  
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

// generate Vector template instance
#include "vector.cc"
template class Vector<SignatureNode>;
