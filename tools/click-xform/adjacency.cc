/*
 * adjacency.cc -- adjacency matrices for Click routers
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

#include "adjacency.hh"
#include "routert.hh"
#include <stdio.h>

AdjacencyMatrix::AdjacencyMatrix(RouterT *r)
  : _x(0)
{
  init(r);
}

AdjacencyMatrix::~AdjacencyMatrix()
{
  delete[] _x;
}

static inline unsigned
type_indicator(ElementClassT *t)
{
  return (unsigned) (reinterpret_cast<uintptr_t>(t));
}

static inline unsigned
connection_indicator(int fromport, int toport)
{
  int p1 = fromport % 16;
  int p2 = toport % 16;
  return (1U<<p1) | (1U<<(p2+16));
}

void
AdjacencyMatrix::init(RouterT *r)
{
  _router = r;
  int n = _n = r->nelements();

  _cap = 0;
  for (int i = 1; i < n; i *= 2)
    _cap++;

  int cap = _cap;
  delete[] _x;
  _x = new unsigned[1<<(2*cap)];

  for (int i = 0; i < (1<<(2*cap)); i++)
    _x[i] = 0;

  _default_match.assign(n, -2);
  ElementClassT *tunnelt = ElementClassT::tunnel_type();
  for (int i = 0; i < r->nelements(); i++) {
    const ElementT *e = r->element(i);
    if (e->type() != tunnelt) {
      _x[i + (i<<cap)] = type_indicator(e->type());
      _default_match[i] = -1;
    }
  }

  // add connections
  for (RouterT::conn_iterator it = r->begin_connections();
       it != r->end_connections(); ++it)
      if (it->from_eindex() != it->to_eindex())
	  _x[it->from_eindex() + (it->to_eindex() << cap)] |=
	      connection_indicator(it->from_port(), it->to_port());

  _output_0_of.clear();
}

void
AdjacencyMatrix::update(const Vector<int> &changed_eindexes)
{
  RouterT *r = _router;
  int cap = _cap;
  if (r->nelements() > (1<<cap)
      || r->nelements() >= r->n_live_elements() + 500) {
    r->remove_dead_elements();
    init(r);
    return;
  }

  _n = r->nelements();

  // clear out columns and rows
  _default_match.resize(_n, -2);
  Vector<int> updated_eindexes(_n, 0);
  ElementClassT *tunnelt = ElementClassT::tunnel_type();
  for (int i = 0; i < changed_eindexes.size(); i++) {
    int j = changed_eindexes[i];
    if (updated_eindexes[j])
      continue;

    // clear column and row
    for (int k = 0; k < (1<<cap); k++)
      _x[ k + (j<<cap) ] = _x[ j + (k<<cap) ] = 0;

    // set type
    ElementClassT *t = r->element(j)->type();
    if (t != tunnelt) {
      _x[ j + (j<<cap) ] = type_indicator(t);
      _default_match[j] = -1;
    } else
      _default_match[j] = -2;

    updated_eindexes[j] = 1;
  }

  // now set new connections
  for (RouterT::conn_iterator it = r->begin_connections();
       it != r->end_connections(); ++it)
      if (it->from_eindex() != it->to_eindex())
	  _x[it->from_eindex() + (it->to_eindex() << cap)] |=
	      connection_indicator(it->from_port(), it->to_port());

  _output_0_of.clear();
}

void
AdjacencyMatrix::init_pattern() const
{
  // checking for a single connection from output 0
  RouterT *r = _router;
  Vector<int> output_0(_n, -1);
  for (RouterT::conn_iterator it = r->begin_connections();
       it != r->end_connections(); ++it)
      if (it->from_port() == 0) {
	  int fromi = it->from_eindex();
	  if (it->to_eindex() == fromi || output_0[fromi] >= 0)
	      output_0[fromi] = -2;
	  else if (output_0[fromi] == -1)
	      output_0[fromi] = it->to_eindex();
      }

  // set _output_0_of
  _output_0_of.assign(_n, -1);
  for (int i = 0; i < _n; i++)
    if (output_0[i] >= 0) {
      int o = output_0[i];
      if (_default_match[o] >= -1 && _default_match[i] >= -1 && o > i)
	_output_0_of[o] = i;
    }
}

void
AdjacencyMatrix::print() const
{
  for (int i = 0; i < _n; i++) {
    for (int j = 0; j < _n; j++)
      fprintf(stderr, "%3x ", _x[(i<<_cap) + j]);
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}

bool
AdjacencyMatrix::next_subgraph_isomorphism(const AdjacencyMatrix *input,
					   Vector<ElementT *> &matchv_e) const
{
  int pat_n = _n;
  int pat_cap = _cap;
  unsigned *pat_x = _x;
  int input_cap = input->_cap;
  unsigned *input_x = input->_x;
  RouterT *input_r = input->_router;

  // assign 'matchv' from 'matchv_e'
  Vector<int> matchv(_default_match);
  int match_eindex;
  int direction;

  if (matchv_e.size() == 0) {
    match_eindex = 0;
    direction = 1;
  } else {
    for (int i = 0; i < matchv.size(); i++)
      if (matchv[i] == -1)
	matchv[i] = matchv_e[i]->eindex();
    match_eindex = pat_n - 1;
    direction = -1;
  }

  int *match = &matchv[0];	// avoid bounds checks
  if (!_output_0_of.size())
    init_pattern();
  int *output_0_of = &_output_0_of[0];

  //print();
  //fprintf(stderr, "input:\n");
  //input->print();

  while (match_eindex >= 0 && match_eindex < pat_n) {
    int rover = match[match_eindex] + 1;
    int max_rover;
    if (rover < 0) {
      match_eindex += direction;
      continue;
    } else if (output_0_of[match_eindex] >= 0) {
      // Speed hack: often we have E1[0] -> [p]E2, the only connection from
      // E1[0], where E1 and E2 are both real elements in the pattern (not
      // 'input' or 'output'). In this case, the match to E2 will be the
      // single element connected from (match[E1])[0]. Find it directly so we
      // don't have to scan over all elements in the input.
      PortT output(input_r->element(match[output_0_of[match_eindex]]), 0);
      RouterT::conn_iterator it = input_r->find_connections_from(output);
      if (rover > 0 || !it.is_back())
	max_rover = -1;
      else {
	rover = it->to().eindex();
	max_rover = rover + 1;
      }
    } else
      max_rover = input->_n;

    while (rover < max_rover) {
      // S_{k,k}(input) =? S_{k,n}(P) * M * (S_{k,n}(P))^T
      // first check the diagonal (where element type)
      if (pat_x[ (match_eindex<<pat_cap) + match_eindex ]
	  != input_x[ (rover<<input_cap) + rover ])
	goto failure;
      // test only the new border
      for (int i = 0; i < match_eindex; i++) {
	int m = match[i];
	if (m >= 0) {
	  unsigned px = pat_x[ (i<<pat_cap) + match_eindex ];
	  unsigned ix = input_x[ (m<<input_cap) + rover ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      for (int j = 0; j < match_eindex; j++) {
	int m = match[j];
	if (m >= 0) {
	  unsigned px = pat_x[ (match_eindex<<pat_cap) + j ];
	  unsigned ix = input_x[ (rover<<input_cap) + m ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      break;

     failure: rover++;
    }

    if (rover < max_rover) {
      match[match_eindex] = rover;
      match_eindex++;
      direction = 1;
    } else {
      match[match_eindex] = -1;
      match_eindex--;
      direction = -1;
    }
  }

  // initialize 'matchv_e' from 'matchv'
  matchv_e.assign(matchv.size(), 0);
  for (int i = 0; i < match_eindex; i++)
    if (match[i] >= 0)
      matchv_e[i] = input_r->element(match[i]);

  //for (int i = 0; i < pat_n; i++) fprintf(stderr,"%d ", match[i]);/* >= 0 ? input->_crap->ename(match[i]).c_str() : "<crap>");*/fputs("\n",stderr);
  return (match_eindex >= 0 ? true : false);
}


bool
check_subgraph_isomorphism(const RouterT *pat, const RouterT *input,
			   const Vector<ElementT *> &match)
{
    // check connections
    for (RouterT::conn_iterator it = pat->begin_connections();
	 it != pat->end_connections(); ++it) {
	int fi = it->from_eindex(), ti = it->to_eindex();
	if (!match[fi] || !match[ti])
	    continue;
	if (!input->has_connection(PortT(match[fi], it->from_port()),
				   PortT(match[ti], it->to_port())))
	    return false;
    }
    return true;
}
