/*
 * adjacency.cc -- adjacency matrices for Click routers
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
#include "adjacency.hh"
#include "routert.hh"
#include <stdio.h>

AdjacencyMatrix::AdjacencyMatrix(RouterT *r, RouterT *type_model = 0)
  : _x(0)
{
  init(r, type_model);
}

AdjacencyMatrix::~AdjacencyMatrix()
{
  delete[] _x;
}

void
AdjacencyMatrix::init(RouterT *r, RouterT *type_model = 0)
{
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
    int t = r->etype(i);
    if (type_model && t >= 0)
      t = type_model->type_index(r->type_name(t));
    if (t >= 0 && t != RouterT::TUNNEL_TYPE) {
      _x[i + (i<<cap)] = t*128;
      _default_match[i] = -1;
    }
  }
  
  const Vector<Hookup> &hf = r->hookup_from();
  const Vector<Hookup> &ht = r->hookup_to();
  for (int i = 0; i < hf.size(); i++)
    // add connections. always add 1 so it's not 0 if the connection is from
    // port 0 to port 0. (DUH!)
    if (hf[i].idx >= 0 && hf[i].idx != ht[i].idx) {
      int p1 = hf[i].port % 32;
      int p2 = (ht[i].port + 16) % 32;
      _x[ hf[i].idx + (ht[i].idx<<cap) ] |= (1U<<p1) | (1U<<p2);
    }
}

void
AdjacencyMatrix::update(RouterT *r, const Vector<int> &changed_eindexes)
{
  int cap = _cap;
  if (r->nelements() > (1<<cap)
      || r->nelements() >= r->real_element_count() + 48) {
    r->remove_blank_elements();
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
    int t = r->etype(j);
    if (t >= 0 && t != RouterT::TUNNEL_TYPE) {
      _x[ j + (j<<cap) ] = t*128;
      _default_match[j] = -1;
    } else
      _default_match[j] = -2;
    
    updated_eindexes[j] = 1;
  }

  // now set new connections
  const Vector<Hookup> &hf = r->hookup_from();
  const Vector<Hookup> &ht = r->hookup_to();
  for (int i = 0; i < hf.size(); i++)
    // add connections. always add 1 so it's not 0 if the connection is from
    // port 0 to port 0. (DUH!)
    if (hf[i].idx >= 0 && hf[i].idx != ht[i].idx) {
      int p1 = hf[i].port % 32;
      int p2 = (ht[i].port + 16) % 32;
      _x[ hf[i].idx + (ht[i].idx<<cap) ] |= (1U<<p1) | (1U<<p2);
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
					   Vector<int> &match) const
{
  int pat_n = _n;
  int pat_cap = _cap;
  unsigned *pat_x = _x;
  int input_n = input->_n;
  int input_cap = input->_cap;
  unsigned *input_x = input->_x;
  
  int match_idx;
  int direction;
  if (match.size() == 0) {
    match = _default_match;
    match_idx = 0;
    direction = 1;
  } else {
    match_idx = pat_n - 1;
    direction = -1;
  }

  //print();
  //fprintf(stderr, "input:\n");
  //input->print();
  
  while (match_idx >= 0 && match_idx < pat_n) {
    int rover = match[match_idx] + 1;
    if (rover < 0) {
      match_idx += direction;
      continue;
    }

    while (rover < input_n) {
      match[match_idx] = rover;
    
      // S_{k,k}(input) =? S_{k,n}(P) * M * (S_{k,n}(P))^T
      // test only the new border
      for (int i = 0; i <= match_idx; i++) {
	int m = match[i];
	if (m >= 0) {
	  unsigned px = pat_x[ (i<<pat_cap) + match_idx ];
	  unsigned ix = input_x[ (m<<input_cap) + match[match_idx] ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      for (int j = 0; j < match_idx; j++) {
	int m = match[j];
	if (m >= 0) {
	  unsigned px = pat_x[ (match_idx<<pat_cap) + j ];
	  unsigned ix = input_x[ (match[match_idx]<<input_cap) + m ];
	  if ((px & ix) != px)
	    goto failure;
	}
      }
      break;
      
     failure: rover++;
    }
    
    if (rover < input_n) {
      match_idx++;
      direction = 1;
    } else {
      match[match_idx] = -1;
      match_idx--;
      direction = -1;
    }
  }

  //for (int i = 0; i < pat_n; i++) fprintf(stderr,"%d ", match[i]);/* >= 0 ? input->_crap->ename(match[i]).cc() : "<crap>");*/fputs("\n",stderr);
  return (match_idx >= 0 ? true : false);
}


bool
check_subgraph_isomorphism(const RouterT *pat, const RouterT *input,
			   const Vector<int> &match)
{
  const Vector<Hookup> &hf = pat->hookup_from();
  const Vector<Hookup> &ht = pat->hookup_to();
  int nh = hf.size();
  for (int i = 0; i < nh; i++) {
    if (match[hf[i].idx] < 0 || match[ht[i].idx] < 0)
      continue;
    if (!input->has_connection(Hookup(match[hf[i].idx], hf[i].port),
			       Hookup(match[ht[i].idx], ht[i].port)))
      return false;
  }
  return true;
}
