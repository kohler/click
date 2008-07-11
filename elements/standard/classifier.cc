/*
 * classifier.{cc,hh} -- element is a generic classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2008 Regents of the University of California
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
#include "classifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

//
// CLASSIFIER::EXPR OPERATIONS
//

bool
Classifier::Expr::implies(const Expr &e) const
  /* Returns true iff a packet that matches `*this' must match `e'. */
{
  if (!e.mask.u)
    return true;
  else if (e.offset != offset)
    return false;
  uint32_t both_mask = mask.u & e.mask.u;
  return both_mask == e.mask.u
    && (value.u & both_mask) == e.value.u;
}

bool
Classifier::Expr::not_implies(const Expr &e) const
  /* Returns true iff a packet that DOES NOT match `*this' must match `e'. */
  /* This happens when (1) 'e' matches everything, or (2) 'e' and '*this'
     both match against the same single bit, and they have different values. */
{
  if (!e.mask.u)
    return true;
  else if (e.offset != offset || (mask.u & (mask.u - 1)) != 0
	   || mask.u != e.mask.u || value.u == e.value.u)
    return false;
  else
    return true;
}

bool
Classifier::Expr::implies_not(const Expr &e) const
  /* Returns true iff a packet that matches `*this' CANNOT match `e'. */
{
  if (!e.mask.u || e.offset != offset)
    return false;
  uint32_t both_mask = mask.u & e.mask.u;
  return both_mask == e.mask.u
    && (value.u & both_mask) != (e.value.u & both_mask);
}

bool
Classifier::Expr::not_implies_not(const Expr &e) const
  /* Returns true iff a packet that DOES NOT match `*this' CANNOT match `e'. */
{
  if (!mask.u)
    return true;
  else if (e.offset != offset)
    return false;
  uint32_t both_mask = mask.u & e.mask.u;
  return both_mask == mask.u
    && (value.u & both_mask) == (e.value.u & both_mask);
}

bool
Classifier::Expr::compatible(const Expr &e) const
{
  if (!mask.u || !e.mask.u)
    return true;
  else if (e.offset != offset)
    return false;
  uint32_t both_mask = mask.u & e.mask.u;
  return (value.u & both_mask) == (e.value.u & both_mask);
}

bool
Classifier::Expr::flippable() const
{
  if (!mask.u)
    return false;
  else
    return ((mask.u & (mask.u - 1)) == 0);
}

void
Classifier::Expr::flip()
{
  assert(flippable());
  value.u ^= mask.u;
  int tmp = j[0];
  j[0] = j[1];
  j[1] = tmp;
}

StringAccum &
operator<<(StringAccum &sa, const Classifier::Expr &e)
{
  char buf[20];
  int offset = e.offset;
  sprintf(buf, "%3d/", offset);
  sa << buf;
  for (int i = 0; i < 4; i++)
    sprintf(buf + 2*i, "%02x", e.value.c[i]);
  sprintf(buf + 8, "%%");
  for (int i = 0; i < 4; i++)
    sprintf(buf + 9 + 2*i, "%02x", e.mask.c[i]);
  sa << buf << "  yes->";
  if (e.yes() <= 0)
      sa << "[" << -e.yes() << "]";
  else
      sa << "step " << e.yes();
  sa << "  no->";
  if (e.no() <= 0)
      sa << "[" << -e.no() << "]";
  else
      sa << "step " << e.no();
  return sa;
}

String
Classifier::Expr::s() const
{
  StringAccum sa;
  sa << *this;
  return sa.take_string();
}

//
// CLASSIFIER ITSELF
//

Classifier::Classifier()
    : _output_everything(-1)
{
}

Classifier::~Classifier()
{
}

//
// COMPILATION
//

// DOMINATOR OPTIMIZER

/* Optimize Classifier decision trees by removing useless branches. If we have
   a path like:

   0: x>=5?  ---Y-->  1: y==2?  ---Y-->  2: x>=6?  ---Y-->  3: ...
       \
        --N-->...

   and every path to #1 leads from #0, then we can move #1's "Y" branch to
   point at state #3, since we know that the test at state #2 will always
   succeed.

   There's an obvious exponential-time algorithm to check this. Namely, given
   a state, enumerate all paths that could lead you to that state; then check
   the test against all tests on those paths. This terminates -- the
   classifier structure is a DAG -- but clearly in exptime.

   We reduce the algorithm to polynomial time by storing a bounded number of
   paths per state. For every state S, we maintain a set of up to
   MAX_DOMLIST==4 path subsets D1...D4, so *every* path to state S is a
   superset of at least one Di. (There is no requirement that S contains Di as
   a contiguous subpath. Rather, Di might leave out edges.) We can then shift
   edges as follows. Given an edge S.x-->T, check whether T is resolved (to
   the same answer) by every one of the path subsets D1...D4 corresponding to
   S. If so, then the edge S.x-->T is redundant; shift it to destination
   corresponding to the answer to T. (In the example above, we shift #1.Y to
   point to #3, since that is the destination of the #2.Y edge.)

   _dom holds all the Di sets for all states.
   _dom_start[k] says where, in _dom, a given Di begins.
   _domlist_start[S] says where, in _dom_start, the list of dominator sets
   for state S begins.
*/


Classifier::DominatorOptimizer::DominatorOptimizer(Classifier *c)
  : _c(c)
{
  _dom_start.push_back(0);
  _domlist_start.push_back(0);
}

inline Classifier::Expr &
Classifier::DominatorOptimizer::expr(int state) const
{
  return _c->_exprs[state];
}

inline int
Classifier::DominatorOptimizer::nexprs() const
{
  return _c->_exprs.size();
}

inline bool
Classifier::DominatorOptimizer::br_implies(int brno, int state) const
{
  assert(state > 0);
  if (br(brno))
    return expr(stateno(brno)).implies(expr(state));
  else
    return expr(stateno(brno)).not_implies(expr(state));
}

inline bool
Classifier::DominatorOptimizer::br_implies_not(int brno, int state) const
{
  assert(state > 0);
  if (br(brno))
    return expr(stateno(brno)).implies_not(expr(state));
  else
    return expr(stateno(brno)).not_implies_not(expr(state));
}

void
Classifier::DominatorOptimizer::find_predecessors(int state, Vector<int> &v) const
{
  for (int i = 0; i < state; i++) {
    Expr &e = expr(i);
    if (e.yes() == state)
      v.push_back(brno(i, true));
    if (e.no() == state)
      v.push_back(brno(i, false));
  }
}

#if CLICK_USERLEVEL
void
Classifier::DominatorOptimizer::print()
{
  String s = Classifier::program_string(_c, 0);
  fprintf(stderr, "%s\n", s.c_str());
  for (int i = 0; i < _domlist_start.size() - 1; i++) {
    if (_domlist_start[i] == _domlist_start[i+1])
      fprintf(stderr, "S-%d   NO DOMINATORS\n", i);
    else {
      fprintf(stderr, "S-%d : ", i);
      for (int j = _domlist_start[i]; j < _domlist_start[i+1]; j++) {
	if (j > _domlist_start[i])
	  fprintf(stderr, "    : ");
	for (int k = _dom_start[j]; k < _dom_start[j+1]; k++)
	  fprintf(stderr, " %d.%c", stateno(_dom[k]), br(_dom[k]) ? 'Y' : 'N');
	fprintf(stderr, "\n");
      }
    }
  }
}
#endif

void
Classifier::DominatorOptimizer::calculate_dom(int state)
{
  assert(_domlist_start.size() == state + 1);
  assert(_dom_start.size() - 1 == _domlist_start.back());
  assert(_dom.size() == _dom_start.back());
  
  // find predecessors
  Vector<int> predecessors;
  find_predecessors(state, predecessors);
  
  // if no predecessors, kill this expr
  if (predecessors.size() == 0) {
    if (state > 0)
	expr(state).j[0] = expr(state).j[1] = -_c->noutputs();
    else {
	assert(state == 0);
	_dom.push_back(brno(state, false));
	_dom_start.push_back(_dom.size());
    }
    _domlist_start.push_back(_dom_start.size() - 1);
    return;
  }

  // collect dominator lists from predecessors
  Vector<int> pdom, pdom_end;
  for (int i = 0; i < predecessors.size(); i++) {
    int p = predecessors[i], s = stateno(p);
    
    // if both branches point at same place, remove predecessor state from
    // tree
    if (i > 0 && stateno(predecessors[i-1]) == s) {
      assert(i == predecessors.size() - 1 || stateno(predecessors[i+1]) != s);
      assert(pdom_end.back() > pdom.back());
      assert(stateno(_dom[pdom_end.back() - 1]) == s);
      pdom_end.back()--;
      continue;
    }

    // append all dom lists to pdom and pdom_end; modify dom array to end with
    // branch 'p'
    for (int j = _domlist_start[s]; j < _domlist_start[s+1]; j++) {
      pdom.push_back(_dom_start[j]);
      pdom_end.push_back(_dom_start[j+1]);
      assert(stateno(_dom[pdom_end.back() - 1]) == s);
      _dom[pdom_end.back() - 1] = p;
    }
  }

  // We now have pdom and pdom_end arrays pointing at predecessors'
  // dominators.

  // If we have too many arrays, combine some of them.
  int pdom_pos = 0;
  if (pdom.size() > MAX_DOMLIST) {
    intersect_lists(_dom, pdom, pdom_end, 0, pdom.size(), _dom);
    _dom.push_back(brno(state, false));
    _dom_start.push_back(_dom.size());
    pdom_pos = pdom.size();	// skip loop
  }

  // Our dominators equal predecessors' dominators.
  for (int p = pdom_pos; p < pdom.size(); p++) {
    for (int i = pdom[p]; i < pdom_end[p]; i++) {
      int x = _dom[i];
      _dom.push_back(x);
    }
    _dom.push_back(brno(state, false));
    _dom_start.push_back(_dom.size());
  }

  _domlist_start.push_back(_dom_start.size() - 1);
}


void
Classifier::DominatorOptimizer::intersect_lists(const Vector<int> &in, const Vector<int> &start, const Vector<int> &end, int pos1, int pos2, Vector<int> &out)
  /* Define subvectors V1...Vk as in[start[i] ... end[i]-1] for each pos1 <= i
     < pos2. This code places an intersection of V1...Vk in 'out'. */
{
  assert(pos1 <= pos2 && pos2 <= start.size() && pos2 <= end.size());
  if (pos1 == pos2)
    return;
  else if (pos2 - pos1 == 1) {
    for (int i = start[pos1]; i < end[pos1]; i++)
      out.push_back(in[i]);
  } else {
    Vector<int> pos(start);
    
    // Be careful about lists that end with something <= 0.
    int x = -1;			// 'x' describes the intersection path.
    
    while (1) {
      int p = pos1, k = 0;
      // Search for an 'x' that is on all of V1...Vk. We step through V1...Vk
      // in parallel, using the 'pos' array (initialized to 'start'). On
      // reaching the end of any of the arrays, exit.
      while (k < pos2 - pos1) {
	while (pos[p] < end[p] && in[pos[p]] < x)
	  pos[p]++;
	if (pos[p] >= end[p])
	  goto done;
	// Stepped past 'x'; current value is a new candidate
	if (in[pos[p]] > x)
	  x = in[pos[p]], k = 0;
	p++;
	if (p == pos2)
	  p = pos1;
	k++;
      }
      // Went through all of V1...Vk without changing x, so it's on all lists
      // (0 will definitely be the first such); add it to 'out' and step
      // through again
      out.push_back(x);
      x++;
    }
   done: ;
  }
}

int
Classifier::DominatorOptimizer::dom_shift_branch(int brno, int to_state, int dom, int dom_end, Vector<int> *collector)
{
  // shift the branch from `brno' to `to_state' as far down as you can, using
  // information from `brno's dominators
  assert(dom_end > dom && stateno(_dom[dom_end - 1]) == stateno(brno));
  _dom[dom_end - 1] = brno;
  if (collector)
    collector->push_back(to_state);

  while (to_state > 0) {
    for (int j = dom_end - 1; j >= dom; j--)
      if (br_implies(_dom[j], to_state)) {
	  to_state = expr(to_state).yes();
	  goto found;
      } else if (br_implies_not(_dom[j], to_state)) {
	  to_state = expr(to_state).no();
	  goto found;
      }
    // not found
    break;
   found:
    if (collector)
      collector->push_back(to_state);
  }

  return to_state;
}

int
Classifier::DominatorOptimizer::last_common_state_in_lists(const Vector<int> &in, const Vector<int> &start, const Vector<int> &end)
{
  assert(start.size() == end.size() && start.size() > 1);
  if (in[end[0] - 1] <= 0) {
    int s = in[end[0] - 1];
    for (int j = 1; j < start.size(); j++)
      if (in[end[j] - 1] != s)
	goto not_end;
    return s;
  }
 not_end:
  Vector<int> intersection;
  intersect_lists(in, start, end, 0, start.size(), intersection);
  return intersection.back();
}

void
Classifier::DominatorOptimizer::shift_branch(int brno)
{
  // shift a branch by examining its dominators
  
  int s = stateno(brno);
  int32_t &to_state = expr(s).j[br(brno)];
  if (to_state <= 0)
    return;

  if (_domlist_start[s] + 1 == _domlist_start[s+1]) {
    // single domlist; faster algorithm
    int d = _domlist_start[s];
    to_state = dom_shift_branch(brno, to_state, _dom_start[d], _dom_start[d+1], 0);
  } else {
    Vector<int> vals, start, end;
    for (int d = _domlist_start[s]; d < _domlist_start[s+1]; d++) {
      start.push_back(vals.size());
      (void) dom_shift_branch(brno, to_state, _dom_start[d], _dom_start[d+1], &vals);
      end.push_back(vals.size());
    }
    to_state = last_common_state_in_lists(vals, start, end);
  }
}

void
Classifier::DominatorOptimizer::run(int state)
{
  assert(_domlist_start.size() == state + 1);
  calculate_dom(state);
  shift_branch(brno(state, true));
  shift_branch(brno(state, false));
}


// OPTIMIZATION

bool
Classifier::remove_unused_states()
{
  // Remove uninteresting exprs
  int first = 0;
  for (int i = 0; _output_everything < 0 && i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    int next = e.yes();
    if (e.yes() == e.no() || e.mask.u == 0) {
      if (i == first && next <= 0)
	_output_everything = e.yes();
      else {
	for (int j = 0; j < i; j++)
	    for (int k = 0; k < 2; k++)
		if (_exprs[j].j[k] == i)
		    _exprs[j].j[k] = next;
	if (i == 0)
	    first = next;
      }
    }
  }
  if (_output_everything < 0 && first > 0)
    _exprs[0] = _exprs[first];

  // Remove unreachable states
  for (int i = 1; i < _exprs.size(); i++) {
    for (int j = 0; j < i; j++)	// all branches are forward
      if (_exprs[j].yes() == i || _exprs[j].no() == i)
	goto done;
    // if we get here, the state is unused
    for (int j = i+1; j < _exprs.size(); j++)
      _exprs[j-1] = _exprs[j];
    _exprs.pop_back();
    for (int j = 0; j < _exprs.size(); j++)
	for (int k = 0; k < 2; k++)
	    if (_exprs[j].j[k] >= i)
		_exprs[j].j[k]--;
    i--;			// shifted downward, so must reconsider `i'
   done: ;
  }

  // Get rid of bad branches
  Vector<int> failure_states(_exprs.size(), FAILURE);
  bool changed = false;
  for (int i = _exprs.size() - 1; i >= 0; i--) {
    Expr &e = _exprs[i];
    for (int k = 0; k < 2; k++)
	if (e.j[k] > 0 && failure_states[e.j[k]] != FAILURE) {
	    e.j[k] = failure_states[e.j[k]];
	    changed = true;
	}
    if (e.yes() == FAILURE)
      failure_states[i] = e.no();
    else if (e.no() == FAILURE)
      failure_states[i] = e.yes();
  }
  return changed;
}

void
Classifier::combine_compatible_states()
{
  for (int i = 0; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.no() > 0 && _exprs[e.no()].compatible(e) && e.flippable())
      e.flip();
    if (e.yes() <= 0)
      continue;
    Expr &ee = _exprs[e.yes()];
    if (e.no() == ee.yes() && ee.flippable())
      ee.flip();
    if (e.no() == ee.no() && ee.compatible(e)) {
      e.yes() = ee.yes();
      if (!e.mask.u)		// but probably ee.mask.u is always != 0...
	e.offset = ee.offset;
      e.value.u = (e.value.u & e.mask.u) | (ee.value.u & ee.mask.u);
      e.mask.u |= ee.mask.u;
      i--;
    }
  }
}

void
Classifier::count_inbranches(Vector<int> &inbranch) const
{
    inbranch.assign(_exprs.size(), -1);
    for (int i = 0; i < _exprs.size(); i++) {
	const Expr &e = _exprs[i];
	for (int k = 0; k < 2; k++)
	    if (e.j[k] > 0)
		inbranch[e.j[k]] = (inbranch[e.j[k]] >= 0 ? 0 : i);
    }
}

void
Classifier::bubble_sort_and_exprs(int sort_stopper)
{
    Vector<int> inbranch;
    count_inbranches(inbranch);
    
    // do bubblesort
    for (int i = 0; i < _exprs.size(); i++) {
	Expr &e1 = _exprs[i];
	for (int k = 0; k < 2; k++)
	    if (e1.j[k] > 0) {
		int j = e1.j[k];
		Expr &e2 = _exprs[j];
		if (e1.j[!k] == e2.j[!k]
		    && (e1.offset > e2.offset
			|| (e1.offset == e2.offset && e1.mask.u > e2.mask.u))
		    && e1.offset < sort_stopper && inbranch[j] > 0) {
		    Expr temp(e2);
		    e2 = e1;
		    e2.j[k] = temp.j[k];
		    e1 = temp;
		    e1.j[k] = j;
		    // step backwards to continue the sort
		    i = (inbranch[i] > 0 ? inbranch[i] - 1 : i - 1);
		    break;
		}
	    }
    }
}

void
Classifier::optimize_exprs(ErrorHandler *errh, int sort_stopper)
{
  // sort 'and' expressions
  bubble_sort_and_exprs(sort_stopper);
  
  //{ String sxx = program_string(this, 0); click_chatter("%s", sxx.c_str()); }

  // optimize using dominators
  {
    DominatorOptimizer dom(this);
    for (int i = 0; i < _exprs.size(); i++)
      dom.run(i);
    //dom.print();
    combine_compatible_states();
    (void) remove_unused_states();
  }

  //{ String sxx = program_string(this, 0); click_chatter("%s", sxx.c_str()); }
  
  // Check for case where all patterns have conflicts: _exprs will be empty
  // but _output_everything will still be < 0. We require that, when _exprs
  // is empty, _output_everything is >= 0.
  if (_exprs.size() == 0 && _output_everything < 0)
    _output_everything = noutputs();
  else if (_output_everything >= 0)
    _exprs.clear();

  // Calculate _safe_length
  _safe_length = 0;
  for (int i = 0; i < _exprs.size(); i++) {
    unsigned off = _exprs[i].offset + UBYTES;
    for (int j = 3; j >= 0; j--, off--)
      if (_exprs[i].mask.c[j])
	break;
    if (off > _safe_length)
      _safe_length = off;
  }
  _safe_length -= _align_offset;

  // Warn on patterns that can't match anything
  Vector<int> used_patterns(noutputs() + 1, 0);
  if (_output_everything >= 0)
    used_patterns[_output_everything] = 1;
  else
    for (int i = 0; i < _exprs.size(); i++)
	for (int k = 0; k < 2; k++)
	    if (_exprs[i].j[k] <= 0)
		used_patterns[-_exprs[i].j[k]] = 1;
  for (int i = 0; i < noutputs(); i++)
    if (!used_patterns[i])
      errh->warning("pattern %d matches no packets", i);

  //{ String sxx = program_string(this, 0); click_chatter("%s", sxx.c_str()); }
}

//
// CONFIGURATION
//

void
Classifier::init_expr_subtree(Vector<int> &tree)
{
  assert(!tree.size());
  tree.push_back(0);
}

void
Classifier::add_expr(Vector<int> &tree, const Expr &e)
{
    if (_exprs.size() < 0x7FFF) {
	_exprs.push_back(e);
	Expr &ee = _exprs.back();
	ee.yes() = SUCCESS;
	ee.no() = FAILURE;
	int level = tree[0];
	tree.push_back(level);
    }
}

void
Classifier::add_expr(Vector<int> &tree, int offset, uint32_t value, uint32_t mask)
{
  Expr e;
  e.offset = offset;
  e.value.u = value & mask;
  e.mask.u = mask;
  add_expr(tree, e);
}

void
Classifier::start_expr_subtree(Vector<int> &tree)
{
  tree[0]++;
}

void
Classifier::redirect_expr_subtree(int first, int last, int success, int failure)
{
  for (int i = first; i < last; i++) {
    Expr &e = _exprs[i];
    if (e.yes() == SUCCESS)
	e.yes() = success;
    else if (e.yes() == FAILURE)
	e.yes() = failure;
    if (e.no() == SUCCESS)
	e.no() = success;
    else if (e.no() == FAILURE)
	e.no() = failure;
  }
}

void
Classifier::finish_expr_subtree(Vector<int> &tree, Combiner combiner,
				int success, int failure)
{
  int level = tree[0];

  // 'subtrees' contains pointers to trees at level 'level'
  Vector<int> subtrees;
  {
    // move backward to parent subtree
    int ptr = _exprs.size();
    while (ptr > 0 && (tree[ptr] < 0 || tree[ptr] >= level))
      ptr--;
    // collect child subtrees
    for (ptr++; ptr <= _exprs.size(); ptr++)
      if (tree[ptr] == level)
	subtrees.push_back(ptr - 1);
  }

  if (subtrees.size()) {

    // combine subtrees

    // first mark all subtrees as next higher level
    tree[subtrees[0] + 1] = level - 1;
    for (int e = subtrees[0] + 2; e <= _exprs.size(); e++)
      tree[e] = -1;

    // loop over expressions
    int t;
    for (t = 0; t < subtrees.size() - 1; t++) {
      int first = subtrees[t];
      int next = subtrees[t+1];

      if (combiner == C_AND)
	redirect_expr_subtree(first, next, next, failure);
      else if (combiner == C_OR)
	redirect_expr_subtree(first, next, success, next);
      else if (combiner == C_TERNARY) {
	if (t < subtrees.size() - 2) {
	  int next2 = subtrees[t+2];
	  redirect_expr_subtree(first, next, next, next2);
	  redirect_expr_subtree(next, next2, success, failure);
	  t++;
	} else			// like C_AND
	  redirect_expr_subtree(first, next, next, failure);
      } else
	redirect_expr_subtree(first, next, success, failure);
    }

    if (t < subtrees.size()) {
      assert(t == subtrees.size() - 1);
      redirect_expr_subtree(subtrees[t], _exprs.size(), success, failure);
    }
  }

  tree[0]--;
}

void
Classifier::negate_expr_subtree(Vector<int> &tree)
{
  // swap 'SUCCESS' and 'FAILURE' within the last subtree
  int level = tree[0];
  int first = _exprs.size() - 1;
  while (first >= 0 && tree[first+1] != level)
    first--;

  for (int i = first; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.yes() == FAILURE)
	e.yes() = SUCCESS;
    else if (e.yes() == SUCCESS)
	e.yes() = FAILURE;
    if (e.no() == FAILURE)
	e.no() = SUCCESS;
    else if (e.no() == SUCCESS)
	e.no() = FAILURE;
  }
}

static void
update_value_mask(int c, int shift, int &value, int &mask)
{
  int v = 0, m = 0xF;
  if (c == '?')
    v = m = 0;
  else if (c >= '0' && c <= '9')
    v = c - '0';
  else if (c >= 'A' && c <= 'F')
    v = c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    v = c - 'a' + 10;
  value |= (v << shift);
  mask |= (m << shift);
}

int
Classifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != noutputs())
	return errh->error("need %d arguments, one per output port", noutputs());
  
  int before = errh->nerrors();

  // set align offset
  {
    int c, o;
    if (AlignmentInfo::query(this, 0, c, o) && c >= 4)
      // want `data - _align_offset' aligned at 4/(o%4)
      _align_offset = (4 - (o % 4)) % 4;
    else {
#if !HAVE_INDIFFERENT_ALIGNMENT
      errh->error("machine is sensitive to alignment: you must run config through click-align");
#endif
      _align_offset = 0;
    }
  }
  
  Vector<int> tree;
  init_expr_subtree(tree);
  start_expr_subtree(tree);
  
  for (int slot = 0; slot < conf.size(); slot++) {
    int i = 0;
    int len = conf[slot].length();
    const char *s = conf[slot].data();

    int slot_branch = _exprs.size();
    Vector<Expr> slot_exprs;

    start_expr_subtree(tree);

    if (s[0] == '-' && len == 1)
      // slot accepting everything
      i = 1;
    
    while (i < len) {
      
      while (i < len && isspace((unsigned char) s[i]))
	i++;
      if (i >= len) break;

      start_expr_subtree(tree);
      
      // negated?
      bool negated = false;
      if (s[i] == '!') {
	negated = true;
	i++;
	while (i < len && isspace((unsigned char) s[i]))
	  i++;
      }
      
      if (i >= len || !isdigit((unsigned char) s[i]))
	return errh->error("pattern %d: expected a digit", slot);

      // read offset
      int offset = 0;
      while (i < len && isdigit((unsigned char) s[i])) {
	offset *= 10;
	offset += s[i] - '0';
	i++;
      }
      
      if (i >= len || s[i] != '/')
	return errh->error("pattern %d: expected '/'", slot);
      i++;

      // scan past value
      int value_pos = i;
      while (i < len && (isxdigit((unsigned char) s[i]) || s[i] == '?'))
	i++;
      int value_end = i;

      // scan past mask
      int mask_pos = -1;
      int mask_end = -1;
      if (i < len && s[i] == '%') {
	i++;
	mask_pos = i;
	while (i < len && (isxdigit((unsigned char) s[i]) || s[i] == '?'))
	  i++;
	mask_end = i;
      }

      // check lengths
      if (value_end - value_pos < 2) {
	errh->error("pattern %d: value has less than 2 hex digits", slot);
	value_end = value_pos;
	mask_end = mask_pos;
      }
      if ((value_end - value_pos) % 2 != 0) {
	errh->error("pattern %d: value has odd number of hex digits", slot);
	value_end--;
	mask_end--;
      }
      if (mask_pos >= 0 && (mask_end - mask_pos) != (value_end - value_pos)) {
	bool too_many = (mask_end - mask_pos) > (value_end - value_pos);
	errh->error("pattern %d: mask has too %s hex digits", slot,
		    (too_many ? "many" : "few"));
	if (too_many)
	  mask_end = mask_pos + value_end - value_pos;
	else
	  value_end = value_pos + mask_end - mask_pos;
      }

      // add values to exprs
      bool first = true;
      offset += _align_offset;
      while (value_pos < value_end) {
	int v = 0, m = 0;
	update_value_mask(s[value_pos], 4, v, m);
	update_value_mask(s[value_pos+1], 0, v, m);
	value_pos += 2;
	if (mask_pos >= 0) {
	  int mv = 0, mm = 0;
	  update_value_mask(s[mask_pos], 4, mv, mm);
	  update_value_mask(s[mask_pos+1], 0, mv, mm);
	  mask_pos += 2;
	  m = m & mv & mm;
	}
	if (first || offset % 4 == 0) {
	  add_expr(tree, (offset / 4) * 4, 0, 0);
	  first = false;
	}
	_exprs.back().mask.c[offset % 4] = m;
	_exprs.back().value.c[offset % 4] = v & m;
	offset++;
      }

      // combine with "and"
      finish_expr_subtree(tree, C_AND);

      if (negated)
	negate_expr_subtree(tree);
    }

    // add fake expr if required
    if (_exprs.size() == slot_branch)
      add_expr(tree, 0, 0, 0);

    finish_expr_subtree(tree, C_AND, -slot);
  }

  finish_expr_subtree(tree, C_OR, -noutputs(), -noutputs());

  //{ String sxx = program_string(this, 0); click_chatter("%s", sxx.c_str()); }
  optimize_exprs(errh);
  //{ String sxx = program_string(this, 0); click_chatter("%s", sxx.c_str()); }
  return (errh->nerrors() == before ? 0 : -1);
}

String
Classifier::program_string(Element *element, void *)
{
  Classifier *c = (Classifier *)element;
  StringAccum sa;
  for (int i = 0; i < c->_exprs.size(); i++) {
    Expr e = c->_exprs[i];
    e.offset -= c->_align_offset;
    sa << (i < 10 ? " " : "") << i << ' ' << e << '\n';
  }
  if (c->_exprs.size() == 0)
    sa << "all->[" << c->_output_everything << "]\n";
  sa << "safe length " << c->_safe_length << "\n";
  sa << "alignment offset " << c->_align_offset << "\n";
  return sa.take_string();
}

void
Classifier::add_handlers()
{
    add_read_handler("program", Classifier::program_string, 0, Handler::CALM);
}

//
// RUNNING
//

void
Classifier::length_checked_push(Packet *p)
{
  const unsigned char *packet_data = p->data() - _align_offset;
  int packet_length = p->length() + _align_offset; // XXX >= MAXINT?
  Expr *ex = &_exprs[0];	// avoid bounds checking
  int pos = 0;
  uint32_t data;
  
  do {
    if (ex[pos].offset+UBYTES > packet_length)
      goto check_length;
    
   length_ok:
    data = *(const uint32_t *)(packet_data + ex[pos].offset);
    data &= ex[pos].mask.u;
    pos = ex[pos].j[data == ex[pos].value.u];
    continue;
    
   check_length:
    if (ex[pos].offset < packet_length) {
      unsigned available = packet_length - ex[pos].offset;
      if (!(ex[pos].mask.c[3]
	    || (ex[pos].mask.c[2] && available <= 2)
	    || (ex[pos].mask.c[1] && available == 1)))
	goto length_ok;
    }
    pos = ex[pos].no();
  } while (pos > 0);
  
  checked_output_push(-pos, p);
}

void
Classifier::push(int, Packet *p)
{
  const unsigned char *packet_data = p->data() - _align_offset;
  Expr *ex = &_exprs[0];	// avoid bounds checking
  int pos = 0;
  
  if (_output_everything >= 0) {
    // must use checked_output_push because the output number might be
    // out of range
    pos = -_output_everything;
    goto found;
  } else if (p->length() < _safe_length) {
    // common case never checks packet length
    length_checked_push(p);
    return;
  }
  
  do {
      uint32_t data = *((const uint32_t *)(packet_data + ex[pos].offset));
      data &= ex[pos].mask.u;
      pos = ex[pos].j[data == ex[pos].value.u];
  } while (pos > 0);
  
 found:
  checked_output_push(-pos, p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(AlignmentInfo)
EXPORT_ELEMENT(Classifier)
ELEMENT_MT_SAFE(Classifier)
