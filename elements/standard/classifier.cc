/*
 * classifier.{cc,hh} -- element is a generic classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

//
// CLASSIFIER::EXPR OPERATIONS
//

#define UBYTES ((int)sizeof(uint32_t))

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
{
  if (!e.mask.u)
    return true;
  else
    return false;
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
  int tmp = yes;
  yes = no;
  no = tmp;
}

StringAccum &
operator<<(StringAccum &sa, const Classifier::Expr &e)
{
  char buf[20];
  int offset = e.offset;
  sprintf(buf, "%3d/", offset);
  sa << buf;
  for (int j = 0; j < 4; j++)
    sprintf(buf + 2*j, "%02x", e.value.c[j]);
  sprintf(buf + 8, "%%");
  for (int j = 0; j < 4; j++)
    sprintf(buf + 9 + 2*j, "%02x", e.mask.c[j]);
  sa << buf << "  yes->";
  if (e.yes <= 0)
    sa << "[" << -e.yes << "]";
  else
    sa << "step " << e.yes;
  sa << "  no->";
  if (e.no <= 0)
    sa << "[" << -e.no << "]";
  else
    sa << "step " << e.no;
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
  : Element(1, 0)
{
  MOD_INC_USE_COUNT;
}

Classifier::~Classifier()
{
  MOD_DEC_USE_COUNT;
}

Classifier *
Classifier::clone() const
{
  return new Classifier;
}

//
// COMPILATION
//

// DOMINATOR OPTIMIZER

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
    if (e.yes == state)
      v.push_back(brno(i, true));
    if (e.no == state)
      v.push_back(brno(i, false));
  }
}

#if CLICK_USERLEVEL
void
Classifier::DominatorOptimizer::print()
{
  String s = Classifier::program_string(_c, 0);
  fprintf(stderr, "%s\n", s.cc());
  for (int i = 0; i < _domlist_start.size() - 1; i++) {
    if (_domlist_start[i] == _domlist_start[i+1])
      fprintf(stderr, "S-%d   NO DOMINATORS\n", i);
    else {
      int done = 0;
      for (int j = _domlist_start[i]; j < _domlist_start[i+1]; j++) {
	fprintf(stderr, (done ? "    ": "S-%d "), i);
	fprintf(stderr, ": ");
	for (int k = _dom_start[j]; k < _dom_start[j+1]; k++)
	  fprintf(stderr, " %d.%c", stateno(_dom[k]), br(_dom[k]) ? 'Y' : 'N');
	fprintf(stderr, "\n");
	done = 1;
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
      expr(state).yes = expr(state).no = -_c->noutputs();
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
    pdom_pos = pdom.size();
  }

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
    int x = -1;
    while (1) {
      int p = pos1, k = 0;
      while (k < pos2 - pos1) {
	while (pos[p] < end[p] && in[pos[p]] < x)
	  pos[p]++;
	if (pos[p] >= end[p])
	  goto done;
	if (in[pos[p]] > x)
	  x = in[pos[p]], k = 0;
	p++;
	if (p == pos2)
	  p = pos1;
	k++;
      }
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
	to_state = expr(to_state).yes;
	goto found;
      } else if (br_implies_not(_dom[j], to_state)) {
	to_state = expr(to_state).no;
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
  int &to_state = (br(brno) ? expr(s).yes : expr(s).no);
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
    int next = e.yes;
    if (e.yes == e.no || e.mask.u == 0) {
      if (i == first && next <= 0)
	_output_everything = e.yes;
      else {
	for (int j = 0; j < i; j++) {
	  Expr &ee = _exprs[j];
	  if (ee.yes == i) ee.yes = next;
	  if (ee.no == i) ee.no = next;
	}
	if (i == 0) first = next;
      }
    }
  }
  if (_output_everything < 0 && first > 0)
    _exprs[0] = _exprs[first];

  // Remove unreachable states
  for (int i = 1; i < _exprs.size(); i++) {
    for (int j = 0; j < i; j++)	// all branches are forward
      if (_exprs[j].yes == i || _exprs[j].no == i)
	goto done;
    // if we get here, the state is unused
    for (int j = i+1; j < _exprs.size(); j++)
      _exprs[j-1] = _exprs[j];
    _exprs.pop_back();
    for (int j = 0; j < _exprs.size(); j++) {
      if (_exprs[j].yes >= i) _exprs[j].yes--;
      if (_exprs[j].no >= i) _exprs[j].no--;
    }
    i--;			// shifted downward, so must reconsider `i'
   done: ;
  }

  // Get rid of bad branches
  Vector<int> failure_states(_exprs.size(), FAILURE);
  bool changed = false;
  for (int i = _exprs.size() - 1; i >= 0; i--) {
    Expr &e = _exprs[i];
    if (e.yes > 0 && failure_states[e.yes] != FAILURE) {
      e.yes = failure_states[e.yes];
      changed = true;
    }
    if (e.no > 0 && failure_states[e.no] != FAILURE) {
      e.no = failure_states[e.no];
      changed = true;
    }
    if (e.yes == FAILURE)
      failure_states[i] = e.no;
    else if (e.no == FAILURE)
      failure_states[i] = e.yes;
  }
  return changed;
}

void
Classifier::combine_compatible_states()
{
  for (int i = 0; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.no > 0 && _exprs[e.no].compatible(e) && e.flippable())
      e.flip();
    if (e.yes <= 0)
      continue;
    Expr &ee = _exprs[e.yes];
    if (e.no == ee.yes && ee.flippable())
      ee.flip();
    if (e.no == ee.no && ee.compatible(e)) {
      e.yes = ee.yes;
      e.value.u = (e.value.u & e.mask.u) | (ee.value.u & ee.mask.u);
      e.mask.u |= ee.mask.u;
      i--;
    }
  }
}

void
Classifier::bubble_sort_and_exprs()
{
  // count inbranches
  Vector<int> inbranch(_exprs.size(), -1);
  for (int i = 0; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.yes > 0)
      inbranch[e.yes] = (inbranch[e.yes] >= 0 ? 0 : i);
    if (e.no > 0)
      inbranch[e.no] = (inbranch[e.no] >= 0 ? 0 : i);
  }

  // do bubblesort
  for (int i = 0; i < _exprs.size(); i++)
    if (_exprs[i].yes > 0) {
      int j = _exprs[i].yes;
      Expr &e1 = _exprs[i], &e2 = _exprs[j];
      if (e1.no == e2.no && e1.offset > e2.offset && inbranch[j] > 0) {
	Expr temp(e2);
	e2 = e1;
	e2.yes = temp.yes;
	e1 = temp;
	e1.yes = j;
	// step backwards to continue the sort
	i = (inbranch[i] > 0 ? inbranch[i] - 1 : i - 1);
      }
    }
}

void
Classifier::optimize_exprs(ErrorHandler *errh)
{
  // sort 'and' expressions
  bubble_sort_and_exprs();
  
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }

  // optimize using dominators
  {
    DominatorOptimizer dom(this);
    for (int i = 0; i < _exprs.size(); i++)
      dom.run(i);
    //dom.print();
    combine_compatible_states();
    (void) remove_unused_states();
  }
  
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
  
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
    for (int i = 0; i < _exprs.size(); i++) {
      if (_exprs[i].yes <= 0) used_patterns[-_exprs[i].yes] = 1;
      if (_exprs[i].no <= 0) used_patterns[-_exprs[i].no] = 1;
    }
  for (int i = 0; i < noutputs(); i++)
    if (!used_patterns[i])
      errh->warning("pattern %d matches no packets", i);

  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
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
  _exprs.push_back(e);
  Expr &ee = _exprs.back();
  ee.yes = SUCCESS;
  ee.no = FAILURE;
  int level = tree[0];
  tree.push_back(level);
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
Classifier::finish_expr_subtree(Vector<int> &tree, bool is_and,
				int success = SUCCESS, int failure = FAILURE)
{
  int level = tree[0];
  Vector<int> subtrees;
  
  for (int i = _exprs.size() - 1; i >= 0; i--)
    if (tree[i+1] == level)
      subtrees.push_back(i);
    else if (tree[i+1] >= 0 && tree[i+1] < level)
      break;

  if (subtrees.size()) {
    int first = subtrees.back();
    
    tree[first+1] = level - 1;
    for (int i = first + 1; i < _exprs.size(); i++)
      tree[i+1] = -1;

    int change_from = (is_and ? SUCCESS : FAILURE);
    while (subtrees.size()) {
      subtrees.pop_back();
      int next = (subtrees.size() ? subtrees.back() : _exprs.size());
      if (!subtrees.size())
	change_from = NEVER;
      /* click_chatter("%d %d   %d %d", first, next, change_from, next); */
      for (int i = first; i < next; i++) {
	Expr &e = _exprs[i];
	if (e.yes == change_from)
	  e.yes = next;
	else if (e.yes == SUCCESS)
	  e.yes = success;
	else if (e.yes == FAILURE)
	  e.yes = failure;
	if (e.no == change_from)
	  e.no = next;
	else if (e.no == SUCCESS)
	  e.no = success;
	else if (e.no == FAILURE)
	  e.no = failure;
      }
      first = next;
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
    if (e.yes == FAILURE)
      e.yes = SUCCESS;
    else if (e.yes == SUCCESS)
      e.yes = FAILURE;
    if (e.no == FAILURE)
      e.no = SUCCESS;
    else if (e.no == SUCCESS)
      e.no = FAILURE;
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
  set_noutputs(conf.size());
  
  _output_everything = -1;
  int before = errh->nerrors();

  // set align offset
  {
    int c, o;
    if (AlignmentInfo::query(this, 0, c, o) && c >= 4)
      // want `data - _align_offset' aligned at 4/(o%4)
      _align_offset = (4 - (o % 4)) % 4;
    else {
#ifndef __i386__
      errh->error("no AlignmentInfo available: you may experience unaligned accesses");
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
      
      while (i < len && isspace(s[i]))
	i++;
      if (i >= len) break;

      start_expr_subtree(tree);
      
      // negated?
      bool negated = false;
      if (s[i] == '!') {
	negated = true;
	i++;
	while (i < len && isspace(s[i]))
	  i++;
      }
      
      if (i >= len || !isdigit(s[i]))
	return errh->error("pattern %d: expected a digit", slot);

      // read offset
      int offset = 0;
      while (i < len && isdigit(s[i])) {
	offset *= 10;
	offset += s[i] - '0';
	i++;
      }
      
      if (i >= len || s[i] != '/')
	return errh->error("pattern %d: expected `/'", slot);
      i++;

      // scan past value
      int value_pos = i;
      while (i < len && (isxdigit(s[i]) || s[i] == '?'))
	i++;
      int value_end = i;

      // scan past mask
      int mask_pos = -1;
      int mask_end = -1;
      if (i < len && s[i] == '%') {
	i++;
	mask_pos = i;
	while (i < len && (isxdigit(s[i]) || s[i] == '?'))
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
      finish_expr_subtree(tree, true);

      if (negated)
	negate_expr_subtree(tree);
    }

    // add fake expr if required
    if (_exprs.size() == slot_branch)
      add_expr(tree, 0, 0, 0);

    finish_expr_subtree(tree, true, -slot);
  }

  finish_expr_subtree(tree, false, -noutputs(), -noutputs());

  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
  optimize_exprs(errh);
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
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
  add_read_handler("program", Classifier::program_string, 0);
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
  goto start;
  
  while (pos > 0) {
    
   start:
    if (ex[pos].offset+UBYTES > packet_length)
      goto check_length;
    
   length_ok:
    if ((*((uint32_t *)(packet_data + ex[pos].offset)) & ex[pos].mask.u)
	== ex[pos].value.u)
      pos = ex[pos].yes;
    else
      pos = ex[pos].no;
    continue;
    
   check_length:
    if (ex[pos].offset < packet_length) {
      unsigned available = packet_length - ex[pos].offset;
      if (!(ex[pos].mask.c[3]
	    || (ex[pos].mask.c[2] && available <= 2)
	    || (ex[pos].mask.c[1] && available == 1)))
	goto length_ok;
    }
    pos = ex[pos].no;
  }
  
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
    if ((*((uint32_t *)(packet_data + ex[pos].offset)) & ex[pos].mask.u)
	== ex[pos].value.u)
      pos = ex[pos].yes;
    else
      pos = ex[pos].no;
  } while (pos > 0);
  
 found:
  checked_output_push(-pos, p);
}



#if 0
// optimization detritus

int
Classifier::check_path(const Vector<int> &path,
		       int ei, int interested, int eventual,
		       bool first, bool yet) const
{
  if (ei > interested && ei != eventual && !yet)
    return FAILURE;
  if (ei < 0 || (ei == 0 && !first))
    return (!yet ? FAILURE : ei);

  const Expr &e = _exprs[ei];
  Vector<int> new_path(path);
  new_path.push_back(ei);
  
  int yes_answer = 0;
  for (int i = 0; i < new_path.size() - 1 && !yes_answer; i++) {
    const Expr &old = _exprs[new_path[i]];
    bool yes = (old.yes == new_path[i+1]);
    if ((yes && old.implies_not(e)) || (!yes && old.not_implies_not(e)))
      yes_answer = FAILURE;
  }
  if (!yes_answer)
    yes_answer = check_path(new_path, e.yes, interested, eventual, false,
			    yet || (ei == interested && e.yes == eventual));
  
  int no_answer = 0;
  for (int i = 0; i < new_path.size() - 1 && !no_answer; i++) {
    const Expr &old = _exprs[new_path[i]];
    bool yes = (old.yes == new_path[i+1]);
    if ((yes && old.implies(e)) || (!yes && old.not_implies(e)))
      no_answer = FAILURE;
  }
  if (!no_answer)
    no_answer = check_path(new_path, e.no, interested, eventual, false,
			   yet || (ei == interested && e.no == eventual));

  //fprintf(stderr, "      ");
  //for (int i=0; i<new_path.size(); i++) fprintf(stderr, " %d", new_path[i]);
  //fprintf(stderr, "%s -> [%d, %d]\n", (yet?"*":""), yes_answer, no_answer);
  
  if (ei == interested)
    return (e.yes == eventual ? yes_answer : no_answer);
  else if (yes_answer != FAILURE && no_answer != FAILURE && yes_answer != no_answer)
    return (ei > eventual ? ei : eventual);
  else
    return (yes_answer != FAILURE ? yes_answer : no_answer);
}

int
Classifier::count_occurrences(const Expr &what, int state, bool first) const
{
  if (state < 0 || (state == 0 && !first))
    return 0;
  const Expr &e = _exprs[state];
  int nyes = count_occurrences(what, e.yes, false);
  int nno = count_occurrences(what, e.no, false);
  return (nyes > nno ? nyes : nno) + (what.implies(e) ? 1 : 0);
}

bool
Classifier::remove_duplicate_states()
{
  // look for duplicate states
  Vector<int> init_duplicates;
  for (int i = 0; i < _exprs.size(); i++) {
    const Expr &e = _exprs[i];
    int dupcount = 0;
    for (int j = i + 1; j < _exprs.size(); j++)
      if (e.implies(_exprs[j]))
	dupcount++;
    if (dupcount)
      init_duplicates.push_back(i);
  }

  // check for real duplicates
  Vector<int> duplicates;
  for (int i = 0; i < init_duplicates.size(); i++)
    if (count_occurrences(_exprs[init_duplicates[i]], 0, true) > 1)
      duplicates.push_back(init_duplicates[i]);
  
  if (!duplicates.size())
    return false;
  
  // expand first duplicate
  int splitter = duplicates[0];
  Expr &splite = _exprs[splitter];
  assert(splite.no > 0 && splite.yes > 0);
  //click_chatter("%s", program_string(this, 0).cc());
  //click_chatter("******** %s", splite.s().cc());
  int orig_nexprs = _exprs.size();
  int orig_no_branch = splite.no;
  splite.no = orig_nexprs;
  for (int i = orig_no_branch; i < orig_nexprs; i++) {
    Expr e = _exprs[i];
    if (e.yes > 0) e.yes += orig_nexprs - orig_no_branch;
    if (e.no > 0) e.no += orig_nexprs - orig_no_branch;
    _exprs.push_back(e);
  }
  click_chatter("%s", program_string(this, 0).cc());
  return true;
}

void
Classifier::unaligned_optimize()
{
  // A simple optimization to catch the common case that two adjacent
  // expressions have one of the forms:
  //   OFF/??XXXXXX    OFF/????XXXX    OFF/??????XX
  // OFF+4/XX??????  OFF+4/XXXX????  OFF+4/XXXXXX??
  // Change this into a single expression like:
  // OFF+1/XXXXXXXX  OFF+2/XXXXXXXX  OFF+3/XXXXXXXX
  // It's a pretty weak optimization, but often effective.
  for (int i = 0; i < _exprs.size() - 1; i++) {
    if (_exprs[i].yes != i+1 || _exprs[i].no != _exprs[i+1].no
	|| _exprs[i].offset + UBYTES != _exprs[i+1].offset)
      continue;
    
    // check to see that masks don't conflict
    int shift = 0;
    while (!_exprs[i].mask.c[shift])
      shift++;
    if (shift == 0)
      continue;
    for (int j = shift; j < 4; j++)
      if (_exprs[i+1].mask.c[j])
	goto done;
    
    // combine expressions
    _exprs[i].offset += shift;
    for (int j = 0; j < 4-shift; j++) {
      _exprs[i].mask.c[j] = _exprs[i].mask.c[j+shift];
      _exprs[i].value.c[j] = _exprs[i].value.c[j+shift];
    }
    for (int j = 4-shift; j < 4; j++) {
      _exprs[i].mask.c[j] = _exprs[i+1].mask.c[j-4+shift];
      _exprs[i].value.c[j] = _exprs[i+1].value.c[j-4+shift];
    }
    _exprs[i].yes = _exprs[i+1].yes;
    
   done: ;
  }
}

#endif
#if 0
#define CLASSIFIER_ITERATIVE 1

#if CLASSIFIER_ITERATIVE

#define BEFORE_YES		0x00000000
#define BEFORE_NO		0x00000001
#define BEFORE_COMBINE		0x00000002
#define STATE_FLAG		0x00000003
#define SET_STATE(x, s)		((x) = ((x) & ~STATE_FLAG) | (s))
#define YET_FLAG		0x00000004
#define YES_OK_FLAG		0x00000008
#define YES_OUTPUT(s)		((s >> 8)  & 0x00000FFF)
#define NO_OUTPUT(s)		((s >> 20) & 0x00000FFF)
#define MK_YES_OUTPUT(i)	((i << 8)  & 0x000FFF00)
#define MK_NO_OUTPUT(i)		((i << 20) & 0xFFF00000)

static void
common_paths(Vector<int> &output, int yes_pos, int no_pos)
{
  assert(yes_pos <= no_pos && no_pos <= output.size());
  Vector<int> result;
  
  int yi = yes_pos, ni = no_pos;
  while (yi < no_pos && ni < output.size()) {
    if (output[yi] == output[ni]) {
      result.push_back(output[ni]);
      yi++;
      ni++;
    }
    // cast to unsigned so that outputs and FAILURE are bigger than
    // internal nodes
    while (yi < no_pos && ni < output.size() && (unsigned)output[yi] < (unsigned)output[ni])
      yi++;
    while (yi < no_pos && ni < output.size() && (unsigned)output[yi] > (unsigned)output[ni])
      ni++;
  }

  if (result.size())
    memcpy(&output[yes_pos], &result[0], sizeof(int) * result.size());
  output.resize(yes_pos + result.size());
}

static void
move_path(Vector<int> &output, int to_pos, int start_pos, int end_pos)
{
  assert(to_pos <= start_pos && start_pos <= end_pos && end_pos <= output.size());
  if (to_pos == start_pos)
    output.resize(end_pos);
  else {
    int len = end_pos - start_pos;
    if (len)
      memmove(&output[to_pos], &output[start_pos], sizeof(int) * len);
    output.resize(to_pos + len);
  }
}

/* The recursive check_path function below is easier to understand than this,
 * but unfortunately, it seems to cause kernel crashes b/c of stack overflows.
 * (Deep recursion.) This iterative version, based on the recursive version,
 * should avoid such problems.
 *
 * The iterative transformation is pretty conventional. To transofrm a
 * recursive function to iterative version, you explicitly store the required
 * persistent state from each activation record.
 *
 * The check_path function walks over a path through the _exprs array. Since
 * _exprs is noncircular -- every branch points forward (or to an output) --
 * each path has a maximum length: _exprs.size() + 1. Since only one path is
 * active at a time, the recursion has a maximum depth of _exprs.size(), and
 * we can reserve _exprs.size() state words.
 *
 * What state do we need to keep? The list of current activations, obviously.
 * And for each activation, the state it is in; whether 'yet' is true; whether
 * the recursive call for the 'yes' branch succeeded; and the 'yes_output' and
 * 'no_output' arrays. We store them as follows:
 *
 * - List of prior activations: Each activation corresponds to a single
 *   expression number. Expression numbers are stored in 'path'. When 'path'
 *   is empty the recursion is done. Making a recursive call == adding an
 *   expr# to the end of 'path'. Returning from an activation == removing the
 *   back end of 'path'. Current activation == back end of path.
 *
 * - State the activation is in: Three valid states, "BEFORE_YES" (have not
 *   yet made recursive call along yes branch, the initial state), "BEFORE_NO"
 *   (have not yet made recursive call along no branch), "BEFORE_COMBINE"
 *   (after both recursive calls but before return). Stored in 'flags[expr#] &
 *   STATE_FLAG'.
 *
 * - Whether 'yet' is true: Stored in 'flags[expr#] & YET_FLAG'.
 *
 * - Whether the recursive call for the 'yes' branch succeeded: Stored in
 *   'flags[expr#] & YES_OK_FLAG'.
 *
 * - The 'yes_output' and 'no_output' arrays: Stored in 'output'. Say that an
 *   activation starts with 'output.size() == X'. Then the iterative
 *   activation, like the recursive activation, will return having appended
 *   some vector (possibly empty) to 'output'. However, there is a difference.
 *   The recursive version passes new vectors to recursive calls. Here, we
 *   simply tack more data on to the single 'output' vector, and use indexes
 *   to tell how long the recursive vectors would have been. Specifically, in
 *   the "BEFORE_COMBINE" state, the vector 'yes_answer' is stored in 'output'
 *   indices 'YES_OUTPUT(flags[expr#]) <= i < NO_OUTPUT(flags[expr#])', and
 *   the vector 'no_answer' is stored in 'output' indices
 *   'NO_OUTPUT(flags[expr#]) <= i < output.size()'. Before "BEFORE_COMBINE"
 *   returns, it moves data around, and probably shrinks the 'output' vector,
 *   so that its return value is as required.
 *
 * The return value for the most recently completed activation record is
 * stored in 'bool result'.
 * */

bool
Classifier::check_path_iterative(Vector<int> &output,
				 int interested, int eventual) const
{
  Vector<int> flags(_exprs.size(), 0);
  Vector<int> path;
  path.reserve(_exprs.size() + 1);
  path.push_back(0);

  bool result = false;		// result of last check_path execution

  // loop until path is empty
  while (path.size()) {

    int ei = path.back();
    int flag = flags[ei];
    bool yet = (flag & YET_FLAG) != 0;
    int state = (flag & STATE_FLAG);

    switch (state) {

     case BEFORE_YES: {
       // check for early breakout
       result = false;
       if (ei > interested && ei != eventual && !yet)
	 goto back_up;
       if (yet)
	 output.push_back(ei);
       assert(ei >= 0);
       assert(!(flag & YES_OK_FLAG) && YES_OUTPUT(flag) == 0);

       // store 'YES_OUTPUT'
       flags[ei] = flag = flag | MK_YES_OUTPUT(output.size());

       const Expr &e = _exprs[ei];
       for (int i = 0; i < path.size() - 1; i++) {
	 const Expr &old = _exprs[path[i]];
	 bool yes = (old.yes == path[i+1]);
	 if ((yes && old.implies_not(e)) || (!yes && old.not_implies_not(e)))
	   goto yes_dead;
       }

       // if we get here, must check `yes' branch
       flags[ei] = (flag & ~STATE_FLAG) | BEFORE_NO;       
       if (ei == interested && e.yes == eventual)
	 yet = true;
       ei = e.yes;
       goto step_forward;
     }

     yes_dead:
     case BEFORE_NO: {
       // store 'NO_OUTPUT'
       flags[ei] = flag = flag | MK_NO_OUTPUT(output.size()) | (result ? YES_OK_FLAG : 0);
       result = false;

       const Expr &e = _exprs[ei];
       for (int i = 0; i < path.size() - 1; i++) {
	 const Expr &old = _exprs[path[i]];
	 bool yes = (old.yes == path[i+1]);
	 if ((yes && old.implies(e)) || (!yes && old.not_implies(e)))
	   goto no_dead;
       }

       // if we get here, must check no branch
       flags[ei] = (flag & ~STATE_FLAG) | BEFORE_COMBINE;
       if (ei == interested && e.no == eventual)
	 yet = true;
       ei = e.no;
       goto step_forward;
     }

     step_forward: {
       // move to 'ei'; check for output port rather than internal node
       if (ei <= 0) {
	 if (yet)
	   output.push_back(ei);
	 result = yet;
	 /* do not move forward */
       } else {
	 flags[ei] = BEFORE_YES | (yet ? YET_FLAG : 0);
	 path.push_back(ei);
       }
       break;
     }
     
     no_dead:
     case BEFORE_COMBINE: {
       const Expr &e = _exprs[ei];
       bool yes_ok = ((flag & YES_OK_FLAG) != 0);
       bool no_ok = result;
       
       if (ei == interested) {
	 if (e.yes == eventual)
	   move_path(output, YES_OUTPUT(flag), YES_OUTPUT(flag), NO_OUTPUT(flag));
	 else
	   move_path(output, YES_OUTPUT(flag), NO_OUTPUT(flag), output.size());
	 result = (e.yes == eventual ? yes_ok : no_ok);
	 
       } else if (yes_ok && no_ok) {
	 common_paths(output, YES_OUTPUT(flag), NO_OUTPUT(flag));
	 result = true;
	 
       } else if (!yes_ok && !no_ok)
	 result = false;
       
       else if (yes_ok) {
	 move_path(output, YES_OUTPUT(flag), YES_OUTPUT(flag), NO_OUTPUT(flag));
	 result = true;

       } else {			// no_ok
	 move_path(output, YES_OUTPUT(flag), NO_OUTPUT(flag), output.size());
	 result = true;
       }

       goto back_up;
     }

     back_up: {
       path.pop_back();
       break;
     }

     default:
      assert(0);
      
    }
  }

  return result;
}

#else /* !CLASSIFIER_ITERATIVE */

static void
common_paths(const Vector<int> &a, const Vector<int> &b, Vector<int> &out)
{
  int ai = 0, bi = 0;
  while (ai < a.size() && bi < b.size()) {
    if (a[ai] == b[bi]) {
      out.push_back(a[ai]);
      ai++;
      bi++;
    }
    // cast to unsigned so that outputs and FAILURE are bigger than
    // internal nodes
    while (ai < a.size() && bi < b.size() && (unsigned)a[ai] < (unsigned)b[bi])
      ai++;
    while (ai < a.size() && bi < b.size() && (unsigned)a[ai] > (unsigned)b[bi])
      bi++;
  }
}

bool
Classifier::check_path(const Vector<int> &path, Vector<int> &output,
		       int ei, int interested, int eventual,
		       bool first, bool yet) const
{
  if (ei > interested && ei != eventual && !yet)
    return false;
  if (yet)
    output.push_back(ei);
  if (ei < 0 || (ei == 0 && !first))
    return yet;

  const Expr &e = _exprs[ei];
  Vector<int> new_path(path);
  new_path.push_back(ei);

  Vector<int> yes_answer;
  bool yes_ok = false;
  for (int i = 0; i < new_path.size() - 1; i++) {
    const Expr &old = _exprs[new_path[i]];
    bool yes = (old.yes == new_path[i+1]);
    if ((yes && old.implies_not(e)) || (!yes && old.not_implies_not(e)))
      goto yes_dead;
  }
  yes_ok = check_path(new_path, yes_answer, e.yes, interested, eventual, false,
		      yet || (ei == interested && e.yes == eventual));

 yes_dead:
  Vector<int> no_answer;
  bool no_ok = false;
  for (int i = 0; i < new_path.size() - 1; i++) {
    const Expr &old = _exprs[new_path[i]];
    bool yes = (old.yes == new_path[i+1]);
    if ((yes && old.implies(e)) || (!yes && old.not_implies(e)))
      goto no_dead;
  }
  no_ok = check_path(new_path, no_answer, e.no, interested, eventual, false,
		     yet || (ei == interested && e.no == eventual));

 no_dead:
  //fprintf(stderr, "      ");
  //for (int i=0; i<new_path.size(); i++) fprintf(stderr, " %d", new_path[i]);
  //fprintf(stderr, "%s -> \n", (yet?"*":""));
  
  if (ei == interested) {
    const Vector<int> &v = (e.yes == eventual ? yes_answer : no_answer);
    for (int i = 0; i < v.size(); i++)
      output.push_back(v[i]);
    return (e.yes == eventual ? yes_ok : no_ok);
    
  } else if (yes_ok && no_ok) {
    common_paths(yes_answer, no_answer, output);
    return true;
    
  } else if (!yes_ok && !no_ok)
    return false;
  
  else {
    const Vector<int> &v = (yes_ok ? yes_answer : no_answer);
    for (int i = 0; i < v.size(); i++)
      output.push_back(v[i]);
    return true;
  }
}

#endif /* CLASSIFIER_ITERATIVE */

int
Classifier::check_path(int ei, bool yes) const
{
  int next_ei = (yes ? _exprs[ei].yes : _exprs[ei].no);
  //fprintf(stderr, "%d.%s -> %d\n", ei, yes?"y":"n", next_ei);
  if (next_ei > 0) {
    Vector<int> x;
#if CLASSIFIER_ITERATIVE
    check_path_iterative(x, ei, next_ei);
#else
    check_path(Vector<int>(), x, 0, ei, next_ei, true, false);
#endif
    next_ei = (x.size() ? x.back() : FAILURE);
  }
  // next_ei = check_path(Vector<int>(), 0, ei, next_ei, true, false);
  //fprintf(stderr, "   -> %d\n", next_ei);
  return next_ei;
}

void
Classifier::drift_expr(int ei)
{
  Expr &e = _exprs[ei];
  // only do it once; repetitions without other changes to the dag would be
  // redundant
  e.yes = check_path(ei, true);
  e.no = check_path(ei, false);
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
}
#endif

#if 0
void
Classifier::sort_and_expr_subtree(int from, int success, int failure)
{
  // This function checks the last subtree in _exprs, from `from' to the end
  // of _exprs, to see if it is an AND subtree. Such a subtree can be divided
  // into N sections, where all links inside each section K satisfy the
  // following properties:
  //
  // -- Each "yes" link either remains within section K, or jumps to the
  //    beginning of section K + 1, or (if there is no section K + 1) jumps
  //    to `success'.
  // -- Each "no" link either remains within section K or jumps to `failure'.
  //
  // The sections within such a subtree can be arbitrarily reordered without
  // affecting the subtree's semantics. This function finds such subtrees and
  // sorts them by offset of the first expr in each section. (Thus, the offset
  // of the first expr in section 0 <= the offset of the first expr in section
  // 1, and so forth.) This improves the action of later optimizations.
  
  int nexprs = _exprs.size();
  // 'id' identifies section equivalence classes: if id[i] == id[j], then i
  // and j are in the same section
  Vector<int> id(nexprs, 0);
  for (int i = from; i < nexprs; i++)
    id[i] = i;

  // determine equivalence classes (the sections)
  bool changed;
  do {
    changed = false;
    for (int i = from; i < nexprs; i++) {
      const Expr &e = _exprs[i];
      if (e.no != failure && e.no > 0 && id[i] != id[e.no]) {
	for (int j = i + 1; j <= e.no; j++)
	  id[j] = id[i];
	changed = true;
      } else if (e.yes > 0 && id[i] != id[e.yes - 1]) {
	for (int j = i + 1; j < e.yes; j++)
	  id[j] = id[i];
	changed = true;
      } else if ((e.no <= 0 && e.no != failure) || (e.yes <= 0 && e.yes != success))
	return;
    }
  } while (changed);

  // check for bad branches that would invalidate the transformation
  for (int i = from; id[i] < id.back(); i++) {
    const Expr &e = _exprs[i];
    if (e.yes == success)
      return;
  }
  
  //{ StringAccum sa;
  //sa << success << " -- " << failure << "\n";
  //for (int i = from; i < nexprs; i++) {
  //  sa << (i < 10 ? ">> " : ">>") << i << " [" << (id[i] < 10 ? " " : "") << id[i] << "] " << _exprs[i] << "\n";
  //}
  //click_chatter("%s", sa.cc()); }

  // extract equivalence classes from 'id' array
  Vector<int> equiv_classes;
  for (int i = from, c = -1; i < nexprs; i++)
    if (id[i] != c) {
      equiv_classes.push_back(i);
      c = id[i];
    }
  if (equiv_classes.size() <= 1)
    return;

  // sort equivalence classes
  bool sorted = true;
  Vector<int> sort_equiv_class(equiv_classes.size(), 0);
  for (int i = 0; i < equiv_classes.size(); i++) {
    int c = equiv_classes[i];
    int j = 0;
    for (; j < i
	   && _exprs[sort_equiv_class[j]].offset <= _exprs[c].offset; j++)
      /* nada */;
    if (j == i)
      sort_equiv_class[i] = c;
    else {
      memmove(&sort_equiv_class[j+1], &sort_equiv_class[j], (i - j) * sizeof(int));
      sort_equiv_class[j] = c;
      sorted = false;
    }
  }

  // return early if already sorted
  if (sorted)
    return;

  // sort the actual exprs
  equiv_classes.push_back(nexprs);
  Vector<Expr> newe;
  for (int i = 0; i < sort_equiv_class.size(); i++) {
    int c = sort_equiv_class[i];
    int new_c = from + newe.size();
    int classno;
    for (classno = 0; equiv_classes[classno] != c; classno++) ;
    int next = (classno == equiv_classes.size() - 2 ? success : equiv_classes[classno+1]);
    int new_next = (i == sort_equiv_class.size() - 1 ? success : new_c + equiv_classes[classno+1] - c);
    for (int j = c; j < nexprs && id[j] == c; j++) {
      Expr e = _exprs[j];
      if (e.yes == next)
	e.yes = new_next;
      else if (e.yes > 0) {
	assert(e.yes >= c && (next <= 0 || e.yes < next));
	e.yes += new_c - c;
      }
      if (e.no > 0) {
	assert(e.no >= c && (next <= 0 || e.no < next));
	e.no += new_c - c;
      } else
	assert(e.no == failure);
      newe.push_back(e);
    }
  }

  memcpy(&_exprs[from], &newe[0], newe.size() * sizeof(Expr));
  
  //{ StringAccum sa;
  //for (int i = from; i < nexprs; i++) {
  //  sa << (i < 10 ? " " : "") << i << " " << _exprs[i] << "\n";
  //}
  //click_chatter("%s", sa.cc()); }
}
#endif


#undef UBYTES
ELEMENT_REQUIRES(AlignmentInfo)
EXPORT_ELEMENT(Classifier)
ELEMENT_MT_SAFE(Classifier)

// generate Vector template instance
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<Classifier::Expr>;
#endif
