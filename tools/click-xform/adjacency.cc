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
type_indicator(int t)
{
  return t;
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
  for (int i = 0; i < n; i++) {
    int t = r->etype_uid(i);
    if (t >= 0 && t != ElementClassT::TUNNEL_UID) {
      _x[i + (i<<cap)] = type_indicator(t);
      _default_match[i] = -1;
    }
  }

  // add connections
  int nh = r->nconnections();
  if (nh) {
    // avoid bounds checks
    const ConnectionT *conn = &(r->connections()[0]);
    for (int i = 0; i < nh; i++)
      if (conn[i].live() && conn[i].from_idx() != conn[i].to_idx())
	_x[ conn[i].from_idx() + (conn[i].to_idx()<<cap) ] |=
	  connection_indicator(conn[i].from_port(), conn[i].to_port());
  }

  _output_0_of.clear();
}

void
AdjacencyMatrix::update(const Vector<int> &changed_eindexes)
{
  RouterT *r = _router;
  int cap = _cap;
  if (r->nelements() > (1<<cap)
      || r->nelements() >= r->real_element_count() + 500) {
    r->remove_dead_elements();
    init(r);
    return;
  }

  _n = r->nelements();
  
  // clear out columns and rows
  _default_match.resize(_n, -2);
  Vector<int> updated_eindexes(_n, 0);
  for (int i = 0; i < changed_eindexes.size(); i++) {
    int j = changed_eindexes[i];
    if (updated_eindexes[j])
      continue;

    // clear column and row
    for (int k = 0; k < (1<<cap); k++)
      _x[ k + (j<<cap) ] = _x[ j + (k<<cap) ] = 0;

    // set type
    int t = r->etype_uid(j);
    if (t >= 0 && t != ElementClassT::TUNNEL_UID) {
      _x[ j + (j<<cap) ] = type_indicator(t);
      _default_match[j] = -1;
    } else
      _default_match[j] = -2;
    
    updated_eindexes[j] = 1;
  }

  // now set new connections
  int nh = r->nconnections();
  if (nh) {
    // avoid bounds checks
    const ConnectionT *conn = &(r->connections()[0]);
    for (int i = 0; i < nh; i++)
      if (conn[i].live() && conn[i].from_idx() != conn[i].to_idx())
	_x[ conn[i].from_idx() + (conn[i].to_idx()<<cap) ] |=
	  connection_indicator(conn[i].from_port(), conn[i].to_port());
  }

  _output_0_of.clear();
}

void
AdjacencyMatrix::init_pattern() const
{
  // checking for a single connection from output 0
  RouterT *r = _router;
  Vector<int> output_0(_n, -1);
  const Vector<ConnectionT> &conn = r->connections();
  for (int i = 0; i < conn.size(); i++)
    if (conn[i].live() && conn[i].from_port() == 0) {
      int fromi = conn[i].from_idx();
      if (conn[i].to_idx() == fromi || output_0[fromi] >= 0)
	output_0[fromi] = -2;
      else if (output_0[fromi] == -1)
	output_0[fromi] = conn[i].to_idx();
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

  // assign 'matchv' from 'matchv_e'
  Vector<int> matchv(_default_match);
  int match_idx;
  int direction;
  
  if (matchv_e.size() == 0) {
    match_idx = 0;
    direction = 1;
  } else {
    for (int i = 0; i < matchv.size(); i++)
      if (matchv[i] == -1)
	matchv[i] = matchv_e[i]->idx();
    match_idx = pat_n - 1;
    direction = -1;
  }
  
  int *match = &matchv[0];	// avoid bounds checks
  if (!_output_0_of.size())
    init_pattern();
  int *output_0_of = &_output_0_of[0];
  
  //print();
  //fprintf(stderr, "input:\n");
  //input->print();
  
  while (match_idx >= 0 && match_idx < pat_n) {
    int rover = match[match_idx] + 1;
    int max_rover;
    if (rover < 0) {
      match_idx += direction;
      continue;
    } else if (output_0_of[match_idx] >= 0) {
      // Speed hack: often we have E1[0] -> [p]E2, the only connection from
      // E1[0], where E1 and E2 are both real elements in the pattern (not
      // `input' or `output'). In this case, the match to E2 will be the
      // single element connected from (match[E1])[0]. Find it directly so we
      // don't have to scan over all elements in the input.
      HookupI out;
      if (rover > 0
	  || !input->_router->find_connection_from(HookupI(match[output_0_of[match_idx]], 0), out))
	max_rover = -1;
      else {
	rover = out.idx;
	max_rover = rover + 1;
      }
    } else
      max_rover = input->_n;

    while (rover < max_rover) {
      // S_{k,k}(input) =? S_{k,n}(P) * M * (S_{k,n}(P))^T
      // first check the diagonal (where element type)
      if (pat_x[ (match_idx<<pat_cap) + match_idx ]
	  != input_x[ (rover<<input_cap) + rover ])
	goto failure;
      // test only the new border
      for (int i = 0; i < match_idx; i++) {
	int m = match[i];
	if (m >= 0) {
	  unsigned px = pat_x[ (i<<pat_cap) + match_idx ];
	  unsigned ix = input_x[ (m<<input_cap) + rover ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      for (int j = 0; j < match_idx; j++) {
	int m = match[j];
	if (m >= 0) {
	  unsigned px = pat_x[ (match_idx<<pat_cap) + j ];
	  unsigned ix = input_x[ (rover<<input_cap) + m ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      break;
      
     failure: rover++;
    }
    
    if (rover < max_rover) {
      match[match_idx] = rover;
      match_idx++;
      direction = 1;
    } else {
      match[match_idx] = -1;
      match_idx--;
      direction = -1;
    }
  }

  // initialize 'matchv_e' from 'matchv'
  matchv_e.assign(matchv.size(), 0);
  for (int i = 0; i < match_idx; i++)
    if (match[i] >= 0)
      matchv_e[i] = input->_router->element(match[i]);
  
  //for (int i = 0; i < pat_n; i++) fprintf(stderr,"%d ", match[i]);/* >= 0 ? input->_crap->ename(match[i]).cc() : "<crap>");*/fputs("\n",stderr);
  return (match_idx >= 0 ? true : false);
}


bool
check_subgraph_isomorphism(const RouterT *pat, const RouterT *input,
			   const Vector<ElementT *> &match)
{
  // check connections
  const Vector<ConnectionT> &conn = pat->connections();
  int nh = conn.size();
  for (int i = 0; i < nh; i++) {
    int fi = conn[i].from_idx(), ti = conn[i].to_idx();
    if (!match[fi] || !match[ti])
      continue;
    if (!input->has_connection(Hookup(match[fi], conn[i].from_port()),
			       Hookup(match[ti], conn[i].to_port())))
      return false;
  }
  return true;
}
