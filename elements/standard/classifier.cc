/*
 * classifier.{cc,hh} -- element is a generic classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
#include "bitvector.hh"
#include "straccum.hh"
#include "alignmentinfo.hh"

//
// CLASSIFIER::SPREAD OPERATIONS
//

#define UBYTES ((int)sizeof(unsigned))

Classifier::Spread::Spread()
  : _length(0), _urelevant(new unsigned[32/UBYTES]),
    _uvalue(new unsigned[32/UBYTES])
{
  memset(_urelevant, 0, 32);
}

Classifier::Spread::Spread(const Spread &o)
  : _length(o._length)
{
  int l = (_length < 32 ? 32 : _length);
  _urelevant = new unsigned[l/UBYTES];
  memcpy(_urelevant, o._urelevant, l);
  _uvalue = new unsigned[l/UBYTES];
  memcpy(_uvalue, o._uvalue, l);
}

Classifier::Spread &
Classifier::Spread::operator=(const Spread &o)
{
  delete[] _urelevant;
  delete[] _uvalue;
  Spread sacrifice(o);
  memcpy(this, &sacrifice, sizeof(Spread));
  sacrifice._urelevant = sacrifice._uvalue = 0;
  return *this;
}

Classifier::Spread::~Spread()
{
  delete[] _urelevant;
  delete[] _uvalue;
}

int
Classifier::Spread::grow(int want)
{
  if (want <= _length)
    return 0;
  if (_length <= 0)
    _length = 32;
  int old_length = _length;
  while (_length < want)
    _length *= 2;
  unsigned *new_relevant = new unsigned[_length/UBYTES];
  unsigned *new_value = new unsigned[_length/UBYTES];
  if (!new_relevant || !new_value) {
    delete[] new_relevant;
    delete[] new_value;
    return -1;
  }
  memcpy(new_relevant, _urelevant, old_length);
  memset((char *)new_relevant + old_length, 0, _length - old_length);
  memcpy(new_value, _uvalue, old_length);
  delete[] _urelevant;
  delete[] _uvalue;
  _urelevant = new_relevant;
  _uvalue = new_value;
  return 0;
}

int
Classifier::Spread::add(const Expr &e)
{
  if (_length < 0)
    return 0;
  if (e.offset >= _length && grow(e.offset + 4) < 0)
    return -1;
  _urelevant[e.offset/UBYTES] |= e.mask.u;
  _uvalue[e.offset/UBYTES] &= ~e.mask.u;
  _uvalue[e.offset/UBYTES] |= e.value.u;
  return 0;
}

bool
Classifier::Spread::conflicts(const Expr &e) const
  /* Returns true iff a packet matching `*this' cannot match `e'. */
{
  if (_length < 0)
    return true;
  if (e.offset >= _length)
    return false;
  int o = e.offset/UBYTES;
  int both_relevant = _urelevant[o] & e.mask.u;
  return (_uvalue[o] & both_relevant) != (e.value.u & both_relevant);
}

bool
Classifier::Spread::alw_implies_match(const Expr &e) const
  /* Returns true iff a packet matching `*this' must match `e'. */
{
  if (e.offset >= _length)
    return false;
  int o = e.offset/UBYTES;
  unsigned both_relevant = _urelevant[o] & e.mask.u;
  return both_relevant == e.mask.u
    && (_uvalue[o] & e.mask.u) == e.value.u;
}

bool
Classifier::Spread::nev_implies_no_match(const Expr &e) const
  /* Returns true iff a packet that DOES NOT match `*this' cannot match `e'. */
{
  if (e.offset >= _length)
    return false;
  int o = e.offset/UBYTES;
  unsigned both_relevant = _urelevant[o] & e.mask.u;
  return both_relevant
    && both_relevant == _urelevant[o]
    && (_uvalue[o] & both_relevant) == (e.value.u & both_relevant);
}

void
Classifier::Spread::alw_combine(const Spread &o)
{
  int min_length = (o._length < _length ? o._length : _length);
  _length = min_length;
  for (int i = 0; i < min_length/UBYTES; i++) {
    unsigned both_relevant = _urelevant[i] & o._urelevant[i];
    unsigned different_values = _uvalue[i] ^ o._uvalue[i];
    _urelevant[i] = both_relevant;
    _uvalue[i] &= both_relevant & ~different_values;
  }
  for (int i = min_length/UBYTES; i < 32/UBYTES; i++)
    _urelevant[i] = 0;
}

void
Classifier::Spread::nev_combine(const Spread &alw, const Spread &nev,
				const Spread &cur_alw)
{
  int a_length = alw._length;
  int n_length = nev._length;
  int ca_length = cur_alw._length;
  grow(nev._length);
  
  // merge data from `nev' and `alw' into `*this'
  for (int i = 0; i < _length/UBYTES; i++) {
    
    // if `*this' has no constraints here, but `nev' does; and we know
    // that the current value doesn't match `nev' (because of
    // `cur_alw'), then use `nev'.
    if (!_urelevant[i] && i < ca_length/UBYTES && i < n_length/UBYTES) {
      unsigned mask = nev._urelevant[i] & cur_alw._urelevant[i];
      if ((nev._uvalue[i] & mask) != (cur_alw._uvalue[i] & mask)) {
	_urelevant[i] = nev._urelevant[i];
	_uvalue[i] = nev._uvalue[i];
	continue;
      }
    }
    
    // otherwise, if `*this' has no constraints here, we must skip
    if (!_urelevant[i])
      continue;

    // if `*this' has a constraint here, and the incoming value doesn't match
    // the constraint because of `alw', keep the constraint
    if (i < a_length/UBYTES) {
      unsigned mask = _urelevant[i] & alw._urelevant[i];
      if ((_uvalue[i] & mask) != (alw._uvalue[i] & mask))
	continue;
    }

    // otherwise, if `nev' has no constraints here, we must skip
    if (i >= n_length/UBYTES || !nev._urelevant[i]) {
      _urelevant[i] = 0;
      continue;
    }

    // otherwise, pick the bigger constraint, if they're compatible
    unsigned either_relevant = _urelevant[i] | nev._urelevant[i];
    unsigned different_values = _uvalue[i] ^ nev._uvalue[i];
    _uvalue[i] = (_uvalue[i] & _urelevant[i])
      | (nev._uvalue[i] & nev._urelevant[i]);
    _urelevant[i] = (different_values & either_relevant ? 0 : either_relevant);
  }
}

#if 0 && defined(CLICK_USERLEVEL)
static void
print_spread(Classifier::Spread &s)
{
  unsigned char *relevant = (unsigned char *)s._urelevant;
  unsigned char *value = (unsigned char *)s._uvalue;
  for (int i = 0; i < s._length; i++) {
    if (!relevant[i])
      fprintf(stderr, "??");
    else
      fprintf(stderr, "%02x", value[i]);
    if (i % 4 == 3)
      fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");
}
#endif

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

// the optimization algorithm uses `alw' and `nev' (for `always' and `never')
// to represent information about what kinds of packets can traverse a link.
//
// every packet traversing the link must equal the relevant part of `alw'. 
// NO packet traversing the link can equal the relevant part of `nev'.
// `alw' is whole-cloth -- the packet must equal all relevant parts of `alw'.
// `nev' is piecemeal -- a packet cannot match any 4-byte chunk of `nev'.

int
Classifier::drift_one_edge(const Spread &alw, const Spread &nev, int dst) const
{
  // move dst ahead as far as you can
  while (dst > 0) {
    const Expr &e = _exprs[dst];
    if (alw.alw_implies_match(e))
      dst = e.yes;
    else if (nev.nev_implies_no_match(e) || alw.conflicts(e))
      dst = e.no;
    else if (e.yes == e.no)
      dst = e.yes;
    else
      break;
  }

  // if <= 0, we're done: return it
  if (dst <= 0)
    return dst;

  // otherwise, try to look ahead through the graph
  // XXX may be time-expensive; is there a way to limit lookahead?
  int yes_dst = _exprs[dst].yes;
  if (yes_dst > 0) {
    Spread alw_yes(alw);
    alw_yes.add(_exprs[dst]);
    Spread nev_yes(nev);
    yes_dst = drift_one_edge(alw_yes, nev_yes, yes_dst);
  }

  int no_dst = _exprs[dst].no;
  if (no_dst > 0) {
    Spread alw_no(alw);
    Spread nev_no(nev);
    nev_no.add(_exprs[dst]);
    no_dst = drift_one_edge(alw_no, nev_no, no_dst);
  }

  return (yes_dst == no_dst ? yes_dst : dst);
}

static void
combine_edges(Classifier::Spread &alw, Classifier::Spread &nev,
	      const Classifier::Spread &alw_input,
	      const Classifier::Spread &nev_input, bool any)
{
  if (any) {
    nev.nev_combine(alw_input, nev_input, alw);
    alw.alw_combine(alw_input);
  } else {
    nev = nev_input;
    alw = alw_input;
  }
}

void
Classifier::handle_vertex(int ei, Vector<Spread *> &alw_edges,
			  Vector<Spread *> &nev_edges,
			  Vector<int> &input_counts)
{
  // ei is the expr we're working on
  int nexprs = _exprs.size();
  Expr &e = _exprs[ei];
  
  // find `alw'/`nev' state at entry, by combining data from incoming edges
  Spread alw, nev;
  bool any = false;
  if (ei != 0)		// no inputs to expr 0
    for (int i = 0; i < nexprs; i++) {
      Expr &ee = _exprs[i];
      if (ee.yes == ei) {
	combine_edges(alw, nev, *alw_edges[2*i], *nev_edges[2*i], any);
	any = true;
      }
      if (ee.no == ei) {
	combine_edges(alw, nev, *alw_edges[2*i+1], *nev_edges[2*i+1], any);
	any = true;
      }
    }

  //fprintf(stderr, "** %d\n", ei);
  //fputs("*y ", stderr); print_spread(alw);
  //fputs("*n ", stderr); print_spread(nev);
  
  // calculate edge states
  alw_edges[2*ei] = new Spread(alw);
  alw_edges[2*ei]->add(e);
  nev_edges[2*ei] = new Spread(nev);
  alw_edges[2*ei+1] = new Spread(alw);
  nev_edges[2*ei+1] = new Spread(nev);
  nev_edges[2*ei+1]->add(e);

  //fputs("Yy ", stderr); print_spread(*alw_edges[2*ei]);
  //fputs("Yn ", stderr); print_spread(*nev_edges[2*ei]);
  //fputs("Ny ", stderr); print_spread(*alw_edges[2*ei+1]);
  //fputs("Nn ", stderr); print_spread(*nev_edges[2*ei+1]);
  
  // account for inputs done
  if (e.yes > 0) input_counts[e.yes]--;
  if (e.no > 0) input_counts[e.no]--;
  
  // drift destinations
  e.yes = drift_one_edge(*alw_edges[2*ei], *nev_edges[2*ei], e.yes);
  e.no = drift_one_edge(*alw_edges[2*ei+1], *nev_edges[2*ei+1], e.no);
  
  // reset input_counts[ei] to avoid infinite loop
  input_counts[ei] = -1;
}
			  

void
Classifier::drift_edges()
{
  //fputs(decompile_string(this, 0).cc(), stderr);
  
  // count uncalculated inputs to each expr
  int nexprs = _exprs.size();
  Vector<int> input_counts(nexprs, 0);
  for (int i = 0; i < nexprs; i++) {
    Expr &e = _exprs[i];
    if (e.yes > 0) input_counts[e.yes]++;
    if (e.no > 0) input_counts[e.no]++;
  }
  // don't count unreachable states
  for (int i = 1; i < nexprs; i++)
    if (input_counts[i] == 0)
      input_counts[i] = -1;
  
  // create information about edges
  Vector<Spread *> alw_edges(2*nexprs, (Spread *)0);
  Vector<Spread *> nev_edges(2*nexprs, (Spread *)0);
  
  // loop over all exprs
  while (true) {
    int expr_i = 0;
    while (expr_i < nexprs && input_counts[expr_i] != 0)
      expr_i++;
    if (expr_i >= nexprs) break;
    handle_vertex(expr_i, alw_edges, nev_edges, input_counts);
  }

  // delete spreads
  for (int i = 0; i < 2*nexprs; i++) {
    delete alw_edges[i];
    delete nev_edges[i];
  }
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
Classifier::optimize_exprs(ErrorHandler *errh)
{
  // optimize edges by drifting
  drift_edges();
  
  // Check for case where all patterns have conflicts: _exprs will be empty
  // but _output_everything will still be < 0. We require that, when _exprs
  // is empty, _output_everything is >= 0.
  if (_exprs.size() == 0 && _output_everything < 0)
    _output_everything = noutputs();

#if 0
  // combine adjacent half-full exprs into one full unaligned expr
  unaligned_optimize();
#endif

  // get rid of unused states
  remove_unused_states();
  
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

  //fputs(decompile_string(this, 0).cc(), stderr);
}

//
// CONFIGURATION
//

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
Classifier::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  set_noutputs(args.size());
  
  _output_everything = -1;
  int FAIL = -noutputs();

  // set align offset
  {
    int c, o;
    if (AlignmentInfo::query(this, 0, c, o) && c >= 4)
      _align_offset = o % 4;
    else {
#ifndef __i386__
      errh->error("no AlignmentInfo available: you may experience unaligned accesses");
#endif
      _align_offset = 0;
    }
  }
  
  for (int slot = 0; slot < args.size(); slot++) {
    int i = 0;
    int len = args[slot].length();
    const char *s = args[slot].data();

    int slot_branch = _exprs.size();
    Vector<Expr> slot_exprs;

    if (s[0] == '-' && len == 1) {
      // slot accepting everything
      if (!slot_branch) _output_everything = slot;
      slot_branch = -slot;
      i = 1;
    }
    
    while (i < len) {
      
      while (i < len && isspace(s[i]))
	i++;
      if (i >= len) break;

      // negated?
      bool negated = false;
      if (s[i] == '!') {
	negated = true;
	i++;
      }
      
      if (!isdigit(s[i])) {
	errh->error("expected a digit, got `%c'", s[i]);
	return -1;
      }

      // read offset
      int offset = 0;
      while (i < len && isdigit(s[i])) {
	offset *= 10;
	offset += s[i] - '0';
	i++;
      }
      
      if (i >= len || s[i] != '/') {
	errh->error("expected `/'");
	return -1;
      }
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
	errh->error("slot %d value has less than 2 hex digits", slot);
	value_end = value_pos;
	mask_end = mask_pos;
      }
      if ((value_end - value_pos) % 2 != 0) {
	errh->error("slot %d value has odd number of hex digits", slot);
	value_end--;
	mask_end--;
      }
      if (mask_pos >= 0 && (mask_end - mask_pos) != (value_end - value_pos)) {
	bool too_many = (mask_end - mask_pos) > (value_end - value_pos);
	errh->error("slot %d mask has too %s hex digits", slot,
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
	  Expr e;
	  e.offset = (offset / 4) * 4;
	  e.mask.u = e.value.u = 0;
	  e.yes = slot_branch + slot_exprs.size() + 1;
	  e.no = FAIL;
	  slot_exprs.push_back(e);
	  first = false;
	}
	slot_exprs.back().mask.c[offset % 4] = m;
	slot_exprs.back().value.c[offset % 4] = v & m;
	offset++;
      }

      // switch pointers of last expr in a negated block to implement negation
      if (negated && !first) {
	slot_exprs.back().yes = FAIL;
	slot_exprs.back().no = slot_branch + slot_exprs.size();
      }
    }

    // patch success pointers to point to `slot'
    if (slot_exprs.size()) {
      Expr &e = slot_exprs.back();
      if (e.yes != FAIL) e.yes = -slot;
      if (e.no != FAIL) e.no = -slot;
    }
    
    // patch previous failure pointers to point to this pattern
    for (int ei = 0; ei < _exprs.size(); ei++) {
      Expr &e = _exprs[ei];
      if (e.yes == FAIL) e.yes = slot_branch;
      if (e.no == FAIL) e.no = slot_branch;
    }

    // add slot_exprs to _exprs
    for (int ei = 0; ei < slot_exprs.size(); ei++)
      _exprs.push_back(slot_exprs[ei]);
  }

  optimize_exprs(errh);
  errh->warning(decompile_string(this, 0));
  return 0;
}

String
Classifier::decompile_string(Element *element, void *)
{
  Classifier *f = (Classifier *)element;
  StringAccum sa;
  for (int i = 0; i < f->_exprs.size(); i++) {
    Expr &e = f->_exprs[i];
    char buf[20];
    int offset = e.offset - f->_align_offset;
    sa << i << (offset < 10 ? "   " : "  ") << offset << "/";
    bool need_mask = 0;
    for (int j = 0; j < 4; j++) {
      int m = e.mask.c[j], v = e.value.c[j];
      for (int k = 0; k < 2; k++, m <<= 4, v <<= 4)
	if ((m & 0xF0) == 0x00)
	  sprintf(buf + 2*j + k, "?");
	else {
	  sprintf(buf + 2*j + k, "%x", (v >> 4) & 0xF);
	  if ((m & 0xF0) != 0xF0) need_mask = 1;
	}
    }
    if (need_mask) {
      sprintf(buf + 8, "%%");
      for (int j = 0; j < 4; j++)
	sprintf(buf + 9 + 2*j, "%02x", e.mask.c[j]);
    }
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
  if (f->_exprs.size() == 0)
    sa << "all->[" << f->_output_everything << "]\n";
  sa << "safe length " << f->_safe_length << "\n";
  sa << "alignment offset " << f->_align_offset << "\n";
  return sa.take_string();
}

void
Classifier::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("program", Classifier::decompile_string, 0);
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
  } else if (p->length() + _align_offset < _safe_length) {
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
