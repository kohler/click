/*
 * classifier.{cc,hh} -- element is a generic classifier
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
#include "classifier.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "elements/standard/alignmentinfo.hh"

//
// CLASSIFIER::EXPR OPERATIONS
//

#define UBYTES ((int)sizeof(unsigned))

bool
Classifier::Expr::implies(const Expr &e) const
  /* Returns true iff a packet that matches `*this' must match `e'. */
{
  if (!e.mask.u)
    return true;
  else if (e.offset != offset)
    return false;
  unsigned both_mask = mask.u & e.mask.u;
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
  unsigned both_mask = mask.u & e.mask.u;
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
  unsigned both_mask = mask.u & e.mask.u;
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
  unsigned both_mask = mask.u & e.mask.u;
  return (value.u & both_mask) == (e.value.u & both_mask);
}

//
// CLASSIFIER ITSELF
//

Classifier::Classifier()
  : Element(1, 0)
{
}

Classifier::~Classifier()
{
}

Classifier *
Classifier::clone() const
{
  return new Classifier;
}

//
// COMPILATION
//

// OPTIMIZATION

#if 0

int
Classifier::check_path(int ei, int interested, int eventual,
		       bool first, bool yet) const
{
  if (ei > interested && ei != eventual && !yet)
    return FAILURE;
  if (ei < 0 || (ei == 0 && !first))
    return (!yet ? FAILURE : ei);

  const Expr &e = _exprs[ei];
  const Expr &interest = _exprs[interested];
  bool interest_yes = (interest.yes == eventual);
  if (ei == eventual)
    yet = true;

  int yes_answer = FAILURE, no_answer = FAILURE;
  if (ei < interested || (interest_yes ? !interest.implies_not(e) : !interest.not_implies_not(e)))
    yes_answer = check_path(e.yes, interested, eventual, false, yet);
  if (ei < interested || (interest_yes ? !interest.implies(e) : !interest.not_implies(e)))
    no_answer = check_path(e.no, interested, eventual, false, yet);

  //fprintf(stderr, "   %d -> (%d,%d) ", ei, yes_answer, no_answer);

  if (yes_answer >= eventual) {
    const Expr &event = _exprs[yes_answer];
    if (e.implies(event))
      yes_answer = event.yes;
    else if (e.implies_not(event))
      yes_answer = event.no;
  }
  if (no_answer >= eventual) {
    const Expr &event = _exprs[no_answer];
    if (e.not_implies(event))
      no_answer = event.yes;
    else if (e.not_implies_not(event))
      no_answer = event.no;
  }
  
  //fprintf(stderr, "(%d,%d)\n", yes_answer, no_answer);

  if (ei == interested)
    return (e.yes == eventual ? yes_answer : no_answer);
  else if (yes_answer != FAILURE && no_answer != FAILURE && yes_answer != no_answer)
    return (ei >= eventual ? ei : eventual);
  else
    return (yes_answer != FAILURE ? yes_answer : no_answer);
}

#else

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
  if (ei == eventual)
    yet = true;

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
    yes_answer = check_path(new_path, e.yes, interested, eventual, false, yet);
  
  int no_answer = 0;
  for (int i = 0; i < new_path.size() - 1 && !no_answer; i++) {
    const Expr &old = _exprs[new_path[i]];
    bool yes = (old.yes == new_path[i+1]);
    if ((yes && old.implies(e)) || (!yes && old.not_implies(e)))
      no_answer = FAILURE;
  }
  if (!no_answer)
    no_answer = check_path(new_path, e.no, interested, eventual, false, yet);

  //fprintf(stderr, "(%d,%d)\n", yes_answer, no_answer);
  
  if (ei == interested)
    return (e.yes == eventual ? yes_answer : no_answer);
  else if (yes_answer != FAILURE && no_answer != FAILURE && yes_answer != no_answer)
    return (ei >= eventual ? ei : eventual);
  else
    return (yes_answer != FAILURE ? yes_answer : no_answer);
}

#endif

int
Classifier::check_path(int ei, bool yes) const
{
  int next_ei = (yes ? _exprs[ei].yes : _exprs[ei].no);
  //fprintf(stderr, "%d.%s -> %d\n", ei, yes?"y":"n", next_ei);
  if (next_ei > 0)
    next_ei = check_path(Vector<int>(), 0, ei, next_ei, true, false);
  //fprintf(stderr, "      %d.%s -> %d\n", ei, yes?"y":"n", next_ei);
  return next_ei;
}

void
Classifier::drift_expr(int ei)
{
  Expr &e = _exprs[ei];
  while (1) {
    Expr save(e);
    e.yes = check_path(ei, true);
    e.no = check_path(ei, false);
    if (save.yes == e.yes && save.no == e.no)
      break;
  }
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
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

void
Classifier::remove_unused_states()
{
  // remove uninteresting expr[0]s
  while (_output_everything < 0
	 && (_exprs[0].yes == _exprs[0].no || _exprs[0].mask.u == 0)) {
    int next = _exprs[0].yes;
    if (next <= 0)
      _output_everything = -next;
    else
      _exprs[0] = _exprs[next];
  }
  
  // Now remove unreachable states.
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
}

void
Classifier::combine_compatible_states()
{
  for (int i = 0; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.yes > 0 && e.no == _exprs[e.yes].no && _exprs[e.yes].compatible(e)) {
      Expr &ee = _exprs[e.yes];
      e.yes = ee.yes;
      e.value.u = (e.value.u & e.mask.u) | (ee.value.u & ee.mask.u);
      e.mask.u |= ee.mask.u;
      i--;
    }
  }
}

void
Classifier::optimize_exprs(ErrorHandler *errh)
{
  // optimize edges by drifting
  for (int i = 0; i < _exprs.size(); i++)
    drift_expr(i);
  
  // get rid of unused states
  combine_compatible_states();
  remove_unused_states();

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

  //fputs(program_string(this, 0).cc(), stderr);
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
Classifier::add_expr(Vector<int> &tree, int offset, unsigned value, unsigned mask)
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
      if (!subtrees.size()) change_from = NEVER;
      /* click_chatter("%d %d   %d %d", first, next, change_from, next); */
      for (int i = first; i < next; i++) {
	Expr &e = _exprs[i];
	if (e.yes == change_from) e.yes = next;
	else if (e.yes == SUCCESS) e.yes = success;
	else if (e.yes == FAILURE) e.yes = failure;
	if (e.no == change_from) e.no = next;
	else if (e.no == SUCCESS) e.no = success;
	else if (e.no == FAILURE) e.no = failure;
      }
      first = next;
    }
  }

  tree[0]--;
}

void
Classifier::negate_expr_subtree(Vector<int> &tree)
{
  int level = tree[0];
  int first = _exprs.size() - 1;
  while (first >= 0 && tree[first+1] != level)
    first--;

  for (int i = first; i < _exprs.size(); i++) {
    Expr &e = _exprs[i];
    if (e.yes == FAILURE) e.yes = SUCCESS;
    else if (e.yes == SUCCESS) e.yes = FAILURE;
    if (e.no == FAILURE) e.no = SUCCESS;
    else if (e.no == SUCCESS) e.no = FAILURE;
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
Classifier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  set_noutputs(conf.size());
  
  _output_everything = -1;

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
  return 0;
}

String
Classifier::program_string(Element *element, void *)
{
  Classifier *c = (Classifier *)element;
  StringAccum sa;
  for (int i = 0; i < c->_exprs.size(); i++) {
    Expr &e = c->_exprs[i];
    char buf[20];
    int offset = e.offset - c->_align_offset;
    sprintf(buf, "%2d %3d/", i, offset);
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
    sa << "\n";
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
    if ((*((unsigned *)(packet_data + ex[pos].offset)) & ex[pos].mask.u)
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
  
  checked_push_output(-pos, p);
}

void
Classifier::push(int, Packet *p)
{
  const unsigned char *packet_data = p->data() - _align_offset;
  Expr *ex = &_exprs[0];	// avoid bounds checking
  int pos = 0;
  
  if (_output_everything >= 0) {
    // must use checked_push_output because the output number might be
    // out of range
    pos = -_output_everything;
    goto found;
  } else if (p->length() < _safe_length) {
    // common case never checks packet length
    length_checked_push(p);
    return;
  }
  
  do {
    if ((*((unsigned *)(packet_data + ex[pos].offset)) & ex[pos].mask.u)
	== ex[pos].value.u)
      pos = ex[pos].yes;
    else
      pos = ex[pos].no;
  } while (pos > 0);
  
 found:
  checked_push_output(-pos, p);
}


ELEMENT_REQUIRES(AlignmentInfo)
EXPORT_ELEMENT(Classifier)

// generate Vector template instance
#include "vector.cc"
template class Vector<Classifier::Expr>;
