/*
 * classification.{cc,hh} -- generic packet classification
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2008 Regents of the University of California
 * Copyright (c) 2010 Meraki, Inc.
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
#include "classification.hh"
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS
namespace Classification {
namespace Wordwise {

//
// WORDWISE INSTRUCTION OPERATIONS
//

bool
Insn::hard_implies_short_ok(bool direction, const Insn &x,
			    bool next_direction, unsigned known_length) const
{
    if (short_output != direction)
	return true;
    unsigned r = required_length();
    return r <= known_length
	|| (r <= x.required_length() && next_direction == x.short_output);
}

bool
Insn::implies(const Insn &x, unsigned known_length) const
{
    if (!x.mask.u)
	return true;
    if (x.offset != offset || !implies_short_ok(true, x, true, known_length))
	return false;
    uint32_t both_mask = mask.u & x.mask.u;
    return both_mask == x.mask.u && (value.u & both_mask) == x.value.u;
}

bool
Insn::not_implies(const Insn &x, unsigned known_length) const
{
    if (!x.mask.u)
	return true;
    if (x.offset != offset || !implies_short_ok(false, x, true, known_length))
	return false;
    return (mask.u & (mask.u - 1)) == 0 && mask.u == x.mask.u
	&& value.u != x.value.u;
}

bool
Insn::implies_not(const Insn &x, unsigned known_length) const
{
    if (!x.mask.u || x.offset != offset
	|| !implies_short_ok(true, x, false, known_length))
	return false;
    uint32_t both_mask = mask.u & x.mask.u;
    return both_mask == x.mask.u && (value.u & both_mask) != x.value.u;
}

bool
Insn::not_implies_not(const Insn &x, unsigned known_length) const
{
    if (!mask.u)
	return true;
    if (x.offset != offset || !implies_short_ok(false, x, false, known_length))
	return false;
    uint32_t both_mask = mask.u & x.mask.u;
    return both_mask == mask.u && value.u == (x.value.u & both_mask);
}

bool
Insn::compatible(const Insn &x, bool consider_short) const
{
    if (!mask.u || !x.mask.u)
	return true;
    if (x.offset != offset
	|| (consider_short && x.short_output != short_output
	    && required_length() < x.required_length()))
	return false;
    uint32_t both_mask = mask.u & x.mask.u;
    return (value.u & both_mask) == (x.value.u & both_mask);
}

bool
Insn::generalizable_or_pair(const Insn &x) const
{
    uint32_t value_diff = value.u ^ x.value.u;
    if (offset && x.offset == offset && mask.u && x.mask.u == mask.u
	&& (short_output || !x.short_output) && yes() == x.yes()
	&& (value_diff & (value_diff - 1)) == 0) {
	Insn test(offset, value.u & ~value_diff, mask.u & ~value_diff);
	return test.required_length() == required_length();
    } else
	return false;
}

bool
Insn::flippable() const
{
    if (!mask.u)
	return false;
    else
	return (mask.u & (mask.u - 1)) == 0;
}

void
Insn::flip()
{
    assert(flippable());
    value.u ^= mask.u;
    int tmp = j[0];
    j[0] = j[1];
    j[1] = tmp;
    short_output = !short_output;
}

static void
jump_accum(StringAccum &sa, int j)
{
    if (j <= j_success)
	sa << '[' << ("X-+"[j - j_never]) << ']';
    else if (j <= 0)
	sa << '[' << (-j) << ']';
    else
	sa << "step " << j;
}

StringAccum &
operator<<(StringAccum &sa, const Insn &e)
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
    jump_accum(sa, e.yes());
    sa << "  no->";
    jump_accum(sa, e.no());
    if (e.short_output)
	sa << "  short->yes";
    return sa;
}

String
Insn::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}


//
// PROGRAM
//

//
// CONFIGURATION
//

Vector<int>
Program::init_subtree() const
{
    Vector<int> tree(_insn.size() + 1, -1);
    tree[0] = 0;
    return tree;
}

void
Program::start_subtree(Vector<int> &tree) const
{
    ++tree[0];
}

void
Program::add_insn(Vector<int> &tree, int offset, uint32_t value, uint32_t mask)
{
    _insn.push_back(Insn(offset, value, mask));
    tree.push_back(tree[0]);
    _output_everything = -1;
}

void
Program::redirect_subtree(int first, int last, int success, int failure)
{
    for (int i = first; i < last; ++i) {
	Insn &in = _insn[i];
	for (int k = 0; k < 2; ++k)
	    if (in.j[k] == j_success)
		in.j[k] = success;
	    else if (in.j[k] == j_failure)
		in.j[k] = failure;
    }
}

void
Program::finish_subtree(Vector<int> &tree, Combiner combiner,
			int success, int failure)
{
    int level = tree[0];

    // 'subtrees' contains pointers to trees at level 'level'
    Vector<int> subtrees;
    {
	// move backward to parent subtree
	int ptr = _insn.size();
	while (ptr > 0 && (tree[ptr] < 0 || tree[ptr] >= level))
	    --ptr;
	// collect child subtrees
	for (++ptr; ptr <= _insn.size(); ++ptr)
	    if (tree[ptr] == level)
		subtrees.push_back(ptr - 1);
    }

    if (subtrees.size()) {

	// combine subtrees

	// first mark all subtrees as next higher level
	tree[subtrees[0] + 1] = level - 1;
	for (int e = subtrees[0] + 2; e <= _insn.size(); e++)
	    tree[e] = -1;

	// loop over expressions
	int t;
	for (t = 0; t < subtrees.size() - 1; t++) {
	    int first = subtrees[t];
	    int next = subtrees[t+1];

	    if (combiner == c_and)
		redirect_subtree(first, next, next, failure);
	    else if (combiner == c_or)
		redirect_subtree(first, next, success, next);
	    else if (combiner == c_ternary) {
		if (t < subtrees.size() - 2) {
		    int next2 = subtrees[t+2];
		    redirect_subtree(first, next, next, next2);
		    redirect_subtree(next, next2, success, failure);
		    t++;
		} else		// like c_and
		    redirect_subtree(first, next, next, failure);
	    } else
		redirect_subtree(first, next, success, failure);
	}

	if (t < subtrees.size()) {
	    assert(t == subtrees.size() - 1);
	    redirect_subtree(subtrees[t], _insn.size(), success, failure);
	}
    }

    --tree[0];
}

void
Program::negate_subtree(Vector<int> &tree, bool flip_short)
{
    // swap 'j_success' and 'j_failure' within the last subtree
    int level = tree[0];
    int first = _insn.size() - 1;
    while (first >= 0 && tree[first+1] != level)
	--first;

    for (int i = first; i >= 0 && i < _insn.size(); i++) {
	Insn &e = _insn[i];
	if (e.yes() == j_failure)
	    e.set_yes(j_success);
	else if (e.yes() == j_success)
	    e.set_yes(j_failure);
	if (e.no() == j_failure)
	    e.set_no(j_success);
	else if (e.no() == j_success)
	    e.set_no(j_failure);
	if (flip_short)
	    e.short_output = !e.short_output;
    }
}


// OPTIMIZATION 1: DOMINATORS

/* The DominatorOptimizer optimizes Classifier decision trees by removing
   useless branches. If we have a path like:

   0: x>=6?  ---Y-->  1: y==2?  ---Y-->  2: x>=6?  ---Y-->  3: ...
       \
        --N-->...

   and every path to #1 leads from #0, then we can move #1's "Y" branch to
   point at state #3, since we know that the test at state #2 will always
   succeed.

   There's an obvious exponential-time algorithm to check this. Namely, given
   a state, enumerate all paths that could lead you to that state; then check
   the test against all tests on those paths. This terminates -- the
   classifier structure is a DAG -- but in exptime.

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
   _dom_start[k] says where, in _dom, Dk begins.
   _domlist_start[S] says where, in _dom_start, the dominator sets for state
   S begin.
   The last element in a dominator list (so, for Dk, _dom[_dom_start[k+1]-1])
   is a placeholder; its value has no persistent meaning and is reset
   frequently.
*/

static int
insn_permute_compare(const void *ap, const void *bp, void *user_data)
{
    int a = *reinterpret_cast<const int *>(ap);
    int b = *reinterpret_cast<const int *>(bp);
    const Insn *in = reinterpret_cast<const Insn *>(user_data);
    int cmp = Insn::compare(in[a], in[b]);
    return cmp ? cmp : a - b;
}

DominatorOptimizer::DominatorOptimizer(Program *p)
    : _p(p), _known_length(_p->ninsn(), 0x7FFFFFFF), _insn_id(_p->ninsn(), 0),
      _dom_start(1, 0), _domlist_start(1, 0)
{
    if (_p->ninsn())
	_known_length[0] = 0;
    for (int i = 0; i < _p->ninsn(); ++i) {
	const Insn &insn = _p->insn(i);
	int tested_length = insn.required_length();
	if (tested_length < _known_length[i])
	    tested_length = _known_length[i];
	bool so = insn.short_output;
	if (insn.j[!so] > 0 && tested_length < _known_length[insn.j[!so]])
	    _known_length[insn.j[!so]] = tested_length;
	if (insn.j[so] > 0 && _known_length[i] < _known_length[insn.j[so]])
	    _known_length[insn.j[so]] = _known_length[i];
    }

    // Map instructions to unique IDs
    Vector<int> insn_permute;
    for (int i = 0; i < _p->ninsn(); ++i)
	insn_permute.push_back(i);
    click_qsort(insn_permute.begin(), insn_permute.size(), sizeof(int),
		insn_permute_compare, (void *) _p->begin());
    for (int i = 0; i < _p->ninsn(); ++i)
	if (i > 0 && Insn::compare(_p->insn(insn_permute[i]), _p->insn(insn_permute[i-1])) == 0)
	    _insn_id[insn_permute[i]] = _insn_id[insn_permute[i-1]];
	else
	    _insn_id[insn_permute[i]] = insn_permute[i];
}

void
DominatorOptimizer::find_predecessors(int state, Vector<int> &v) const
{
#if CLICK_CLASSIFICATION_WORDWISE_DOMINATOR_FASTPRED
    if (!_pred_first.size()) {
	_pred_first.assign(ninsn(), -1);
	_pred_next.assign(ninsn() * 2, -1);
	_pred_prev.assign(ninsn() * 2, -1);
	for (int i = 0; i < ninsn(); ++i) {
	    const Insn &in = insn(i);
	    for (int k = 0; k < 2; ++k)
		if (in.j[k] > 0) {
		    int nexts = in.j[k], br = brno(i, k);
		    _pred_next[br] = _pred_first[nexts];
		    if (_pred_first[nexts] >= 0)
			_pred_prev[_pred_first[nexts]] = br;
		    _pred_first[nexts] = br;
		}
	}
    }

    for (int br = _pred_first[state]; br >= 0; br = _pred_next[br])
	v.push_back(br);

# if 0 /* This code tests that the linked-list predecessors are right */
    Vector<int> vv;
    for (int i = 0; i < state; i++) {
	const Insn &in = insn(i);
	for (int k = 0; k < 2; ++k)
	    if (in.j[k] == state)
		vv.push_back(brno(i, k));
    }

    assert(v.size() == vv.size() && memcmp(v.begin(), vv.begin(), sizeof(int) * v.size()) == 0);
# endif
#else
    for (int i = 0; i < state; i++) {
	const Insn &in = insn(i);
	for (int k = 0; k < 2; ++k)
	    if (in.j[k] == state)
		vv.push_back(brno(i, k));
    }
#endif
}

#if CLICK_USERLEVEL
void
DominatorOptimizer::print()
{
    String s = _p->unparse();
    fprintf(stderr, "%s\n", s.c_str());
    for (int i = 0; i < _domlist_start.size() - 1; i++) {
	if (_insn_id[i] == i)
	    fprintf(stderr, "S%d    ", i);
	else
	    fprintf(stderr, "S%d[=%d]", i, _insn_id[i]);
	if (_domlist_start[i] == _domlist_start[i+1])
	    fprintf(stderr, " :  NO DOMINATORS\n");
	else {
	    fprintf(stderr, " : ");
	    for (int j = _domlist_start[i]; j < _domlist_start[i+1]; ++j) {
		if (j > _domlist_start[i])
		    fprintf(stderr, "       : ");
		int endk = _dom_start[j+1];
		for (int k = _dom_start[j]; k < endk; ++k)
		    fprintf(stderr, k == endk - 1 ? " (%d.%c)" : " %d.%c",
			    stateno(_dom[k]), br_yes(_dom[k]) ? 'Y' : 'N');
		fprintf(stderr, "\n");
	    }
	}
    }
}
#endif

void
DominatorOptimizer::calculate_dom(int state)
{
    assert(_domlist_start.size() == state + 1);
    assert(_dom_start.size() - 1 == _domlist_start.back());
    assert(_dom.size() == _dom_start.back());

    // find predecessors
    Vector<int> predecessors;
    find_predecessors(state, predecessors);

    // collect dominator lists from predecessors
    Vector<int> pdom, pdom_end;
    for (int i = 0; i < predecessors.size(); i++) {
	int pred_br = predecessors[i], s = stateno(pred_br);

	// if both branches point at same place, remove predecessor state from
	// tree
	if (i > 0 && stateno(predecessors[i-1]) == s) {
	    assert(i == predecessors.size() - 1 || stateno(predecessors[i+1]) != _insn_id[s]);
	    assert(pdom_end.back() > pdom.back());
	    assert(stateno(_dom[pdom_end.back() - 1]) == _insn_id[s]);
	    pdom_end.back()--;
	    continue;
	}

	// append all dom list boundaries to pdom and pdom_end; modify dom
	// array to end with the correct branch
	int pred_brid = brno(_insn_id[s], br_yes(pred_br));
	for (int j = _domlist_start[s]; j < _domlist_start[s+1]; j++) {
	    int pos1 = _dom_start[j], pos2 = _dom_start[j+1];
	    for (int k = pos1; k < pos2 - 1; ++k) // XXX time consuming?
		if ((_dom[k] ^ pred_brid) == 1)
		    goto ignore_impossible_path;
	    pdom.push_back(pos1);
	    pdom_end.push_back(pos2);
	    assert(stateno(_dom[pos2 - 1]) == _insn_id[s]);
	    _dom[pos2 - 1] = pred_brid;
	ignore_impossible_path: ;
	}
    }

    // We now have pdom and pdom_end arrays pointing at predecessors'
    // dominators.
    // But we may have eliminated every predecessor path as containing a
    // contradiction.  In that case, this state cannot be reached and should
    // be eliminated.  The pdom/pdom_end arrays will be empty.
    int dom_start_oldsize = _dom_start.size();

    if (pdom.size() > MAX_DOMLIST) {
	// We have too many arrays, combine some of them.
	intersect_lists(_dom, pdom, pdom_end, 0, pdom.size(), _dom);
	_dom.push_back(brno(_insn_id[state], false));
	_dom_start.push_back(_dom.size());
    } else if (!pdom.empty()) {
	// Our dominators equal predecessors' dominators.

	// Check for redundant states, where all dominators have the same
	// value for this state's test.
	int mybr = brno(_insn_id[state], false), num_mybr = 0;

	// Loop over predecessors.
	for (int p = 0; p < pdom.size(); p++) {
	    int endpos = pdom_end[p] - 1, last_pdom_br = _dom[endpos],
		pred_mybr = -1;
	    for (int i = pdom[p]; i <= endpos; ++i) {
		int thisbr = _dom[i];
		// Skip a state that will occur later in the list.
		if (i < endpos && (thisbr ^ last_pdom_br) <= 1)
		    continue;
		// Check if list determines this state's test.
		if ((thisbr ^ mybr) <= 1)
		    pred_mybr = thisbr;
		_dom.push_back(thisbr);
	    }
	    if (num_mybr >= 0 && pred_mybr >= 0
		&& (num_mybr == 0 || mybr == pred_mybr))
		mybr = pred_mybr, ++num_mybr;
	    else
		num_mybr = -1;
	    _dom.push_back(mybr);
	    _dom_start.push_back(_dom.size());
	}

	// If state is redundant, predecessors should skip it.
	if (num_mybr > 0) {
	    int new_state = insn(state).j[mybr & 1];
	    for (int i = 0; i < predecessors.size(); ++i)
		set_branch(stateno(predecessors[i]), br_yes(predecessors[i]),
			   new_state);
	    pdom.clear();	// mark this state as impossible (see below)
	}
    }

    // Set branches of impossible states to "j_never".
    if (pdom.empty()) {
	// Clear out any changes to the dom arrays (redundant states only)
	_dom.resize(_dom_start[dom_start_oldsize - 1]);
	_dom_start.resize(dom_start_oldsize);

	if (state > 0) {
	    for (int k = 0; k < 2; ++k)
		set_branch(state, k, Classification::j_never);
	} else {
	    assert(state == 0);
	    _dom.push_back(brno(state, false));
	    _dom_start.push_back(_dom.size());
	}
    }

    _domlist_start.push_back(_dom_start.size() - 1);
}


void
DominatorOptimizer::intersect_lists(const Vector<int> &in, const Vector<int> &start, const Vector<int> &end, int pos1, int pos2, Vector<int> &out)
  /* For each i, pos1 <= i < pos2, let Vi be in[start[i] ... end[i]-1].
     This code places an intersection of all such Vi in 'out'. */
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
	int x = -1;		// 'x' describes the intersection path.

	while (1) {
	    int p = pos1, k = 0;
	    // Search for an 'x' that is on all of V1...Vk. We step through
	    // V1...Vk in parallel, using the 'pos' array (initialized to
	    // 'start'). On reaching the end of any of the arrays, exit.
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
	    // Went through all of V1...Vk without changing x, so it's on all
	    // lists (0 will definitely be the first such); add it to 'out'
	    // and step through again
	    out.push_back(x);
	    x++;
	}
    done: ;
    }
}

int
DominatorOptimizer::dom_shift_branch(int brno, int to_state, int dom, int dom_end, Vector<int> *collector)
{
    // shift the branch from `brno' to `to_state' as far down as you can,
    // using information from `brno's dominators
    brno = DominatorOptimizer::brno(_insn_id[stateno(brno)], br_yes(brno));
    assert(dom_end > dom && stateno(_dom[dom_end - 1]) == stateno(brno));
    _dom[dom_end - 1] = brno;
    if (collector)
	collector->push_back(to_state);

    while (to_state > 0) {
	for (int j = dom_end - 1; j >= dom; j--)
	    if (br_implies(_dom[j], to_state)) {
		to_state = insn(to_state).yes();
		goto found;
	    } else if (br_implies_not(_dom[j], to_state)) {
		to_state = insn(to_state).no();
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
DominatorOptimizer::last_common_state_in_lists(const Vector<int> &in, const Vector<int> &start, const Vector<int> &end)
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
DominatorOptimizer::shift_branch(int state, bool branch)
{
    // shift a branch by examining its dominators

    int32_t nexts = insn(state).j[branch], new_nexts,
	br = brno(state, branch);

    if (_domlist_start[state] == _domlist_start[state+1] || nexts <= 0)
	// impossible or terminating branch
	new_nexts = nexts;
    else if (_domlist_start[state] + 1 == _domlist_start[state+1]) {
	// single domlist; faster algorithm
	int d = _domlist_start[state];
	new_nexts = dom_shift_branch(br, nexts, _dom_start[d], _dom_start[d+1], 0);
    } else {
	Vector<int> vals, start, end;
	for (int d = _domlist_start[state]; d < _domlist_start[state+1]; d++) {
	    start.push_back(vals.size());
	    (void) dom_shift_branch(br, nexts, _dom_start[d], _dom_start[d+1], &vals);
	    end.push_back(vals.size());
	}
	new_nexts = last_common_state_in_lists(vals, start, end);
    }

    if (new_nexts != nexts)
	set_branch(state, branch, new_nexts);
}

void
DominatorOptimizer::run(int state)
{
    assert(_domlist_start.size() == state + 1);
    calculate_dom(state);
    shift_branch(state, true);
    shift_branch(state, false);
    // click_chatter("%s", _p->unparse().c_str());
}


// OPTIMIZATION 2: SPECIAL CASE OPTIMIZATIONS

void
Program::remove_unused_states()
{
    if (!_insn.size())
	return;

    // Remove uninteresting instructions
    // Pass in reverse so we can skip multiple uninteresting insns at once
    Vector<int> destination(_insn.size(), -1);
    for (int i = _insn.size() - 1; i >= 0; --i) {
	Insn &in = _insn[i];
	// First skip uninteresting insns already found
	for (int k = 0; k < 2; ++k)
	    if (in.j[k] > 0)
		in.j[k] = destination[in.j[k]];
	// Second see if this insn is interesting
	if (in.yes() != in.no() && in.mask.u != 0)
	    destination[i] = i;
	else if (in.yes() <= 0)
	    destination[i] = in.yes();
	else
	    destination[i] = destination[in.yes()];
    }

    // Check first instruction.  Requires special-case logic, since
    // destination[0] == 0 might mean either 0 is interesting, or 0 is
    // uninteresting and _output_everything should be 0.
    _output_everything = -1;
    if (destination[0] > 0)
	_insn[0] = _insn[destination[0]];
    else if (destination[0] < 0)
	_output_everything = -destination[0];
    else if (_insn[0].yes() == 0 && (_insn[0].no() == 0 || !_insn[0].mask.u))
	_output_everything = 0;

    // Remove unreachable states
    // First find which states are reachable and assign them new positions.
    destination.assign(_insn.size(), -1);
    if (_output_everything < 0)
	destination[0] = 0;
    int new_insn_index = 0;
    for (int i = 0; i < _insn.size(); ++i)
	if (destination[i] >= 0) {
	    destination[i] = new_insn_index;
	    new_insn_index++;
	    const Insn &in = _insn[i];
	    for (int k = 0; k < 2; ++k)
		if (in.j[k] > 0)
		    destination[in.j[k]] = 0;
	}

    // Second actually rearrange the instruction list.
    for (int i = 0; i < _insn.size(); ++i)
	if (destination[i] >= 0) {
	    int j = destination[i];
	    if (j != i)
		_insn[j] = _insn[i];
	    Insn &in = _insn[j];
	    for (int k = 0; k < 2; ++k)
		if (in.j[k] > 0)
		    in.j[k] = destination[in.j[k]];
	}
    _insn.erase(_insn.begin() + new_insn_index, _insn.end());
}

void
Program::combine_compatible_states()
{
    for (int i = _insn.size() - 1; i >= 0; --i) {
	Insn &in = _insn[i];
	if (in.no() > 0) {
	    Insn &no_in = _insn[in.no()];
	    if (no_in.compatible(in, false) && in.flippable())
		in.flip();
	    else if (in.generalizable_or_pair(no_in)) {
		uint32_t the_bit = in.value.u ^ no_in.value.u;
		in.value.u &= ~the_bit;
		in.mask.u &= ~the_bit;
		in.set_no(no_in.no());
		++i;
		continue;
	    }
	}
	if (in.yes() <= 0)
	    continue;
	Insn &yes_in = _insn[in.yes()];
	if (in.no() == yes_in.yes() && yes_in.flippable())
	    yes_in.flip();
	if (in.no() == yes_in.no() && yes_in.compatible(in, true)) {
	    in.set_yes(yes_in.yes());
	    if (!in.mask.u)	// but probably yes_in.mask.u is always != 0...
		in.offset = yes_in.offset;
	    in.value.u = (in.value.u & in.mask.u) | (yes_in.value.u & yes_in.mask.u);
	    in.mask.u |= yes_in.mask.u;
	    ++i;
	}
    }
}

void
Program::count_inbranches(Vector<int> &inbranches) const
{
    inbranches.assign(_insn.size(), -1);
    for (int i = 0; i < _insn.size(); i++) {
	const Insn &e = _insn[i];
	for (int k = 0; k < 2; k++)
	    if (e.j[k] > 0)
		inbranches[e.j[k]] = (inbranches[e.j[k]] >= 0 ? 0 : i);
    }
}

inline int
Program::map_offset(int offset, const int *begin, const int *end)
{
    if (begin == end || offset < begin[0] || offset > end[-2])
	return offset;
    else
	return hard_map_offset(offset, begin, end);
}

int
Program::hard_map_offset(int offset, const int *begin, const int *end)
{
    while (begin != end) {
	const int *mid = begin + (((end - begin) >> 2) << 1);
	if (mid[0] == offset)
	    return mid[1];
	else if (mid[0] < offset)
	    begin = mid + 2;
	else
	    end = begin;
    }
    return offset;
}

void
Program::bubble_sort_and_exprs(const int *offset_map_begin,
			       const int *offset_map_end,
			       int last_offset)
{
    Vector<int> inbranch;
    count_inbranches(inbranch);

    // do bubblesort
    for (int i = 0; i < _insn.size(); i++) {
	Insn &e1 = _insn[i];
	for (int k = 0; k < 2; k++) {
	    int j = e1.j[k];
	    if (j <= 0
		|| e1.offset >= static_cast<unsigned>(last_offset)
		|| inbranch[j] <= 0)
		continue;
	    Insn &e2 = _insn[j];
	    if (e1.j[!k] != e2.j[!k])
		continue;
	    int o1 = map_offset(e1.offset, offset_map_begin, offset_map_end),
		o2 = map_offset(e2.offset, offset_map_begin, offset_map_end);
	    if (o1 > o2
		|| (o1 == o2
		    && (ntohl(e1.mask.u) > ntohl(e2.mask.u)
			|| (e1.mask.u == e2.mask.u
			    && ntohl(e1.value.u) > ntohl(e2.value.u))))) {
		Insn temp(e2);
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
Program::optimize(const int *offset_map_begin,
		  const int *offset_map_end,
		  int last_offset)
{
    // sort 'and' expressions
    bubble_sort_and_exprs(offset_map_begin, offset_map_end, last_offset);

    // click_chatter("%s", unparse().c_str());

    // optimize using dominators
    {
	DominatorOptimizer dom(this);
	for (int i = 0; i < _insn.size(); i++)
	    dom.run(i);
	//dom.print();
    }
    combine_compatible_states();
    remove_unused_states();

    // click_chatter("%s", unparse().c_str());

    // Check for case where all patterns have conflicts: _insn will be empty
    // but _output_everything will still be < 0. We require that, when _insn
    // is empty, _output_everything is >= 0.
    if (_insn.size() == 0 && _output_everything < 0)
	_output_everything = -Classification::j_never;
    else if (_output_everything >= 0)
	_insn.clear();

    // Calculate _safe_length
    _safe_length = 0;
    for (int i = 0; i < _insn.size(); i++) {
	unsigned req_len = _insn[i].required_length();
	if (req_len > _safe_length)
	    _safe_length = req_len;
    }
    _safe_length -= _align_offset;

    // click_chatter("%s", unparse().c_str());
}

void
Program::set_failure(int failure)
{
    if (_output_everything == -j_failure) {
        assert(failure <= 0);
        _output_everything = -failure;
    }
    for (int i = 0; i < ninsn(); ++i) {
        Insn& insn = _insn[i];
        for (int k = 0; k < 2; ++k)
            if (insn.j[k] == j_failure)
                insn.j[k] = failure;
    }
}

void
Program::add_or_program(const Program& next_program)
{
    // Append `next_program` to this program as with c_or: failure
    // jumps in this program will jump to `next_program`. Expectation:
    // This program has been part-finished (contains no j_success jumps).

    // If this program sends all output somewhere, ignore next_program.
    if (_output_everything < 0 || _output_everything == -j_failure) {
        // Update this program's unlinked jumps
        int failure = -next_program.output_everything();
        if (failure > 0)
            failure = ninsn();
        set_failure(failure);

        // Add next program
        int offset = ninsn();
        for (int i = 0; i < next_program.ninsn(); ++i)
            _insn.push_back(next_program._insn[i].offset_by(offset));
    }
}


// UNPARSING, ETC.

void
Program::warn_unused_outputs(int noutputs, ErrorHandler *errh) const
{
    Vector<int> used(noutputs, 0);
    if (_output_everything >= 0 && _output_everything < noutputs)
	used[_output_everything] = 1;
    else
	for (int i = 0; i < _insn.size(); ++i)
	    for (int k = 0; k < 2; ++k)
		if (_insn[i].j[k] <= 0 && -_insn[i].j[k] < noutputs)
		    used[-_insn[i].j[k]] = 1;

    for (int i = 0; i < noutputs; ++i)
	if (!used[i])
	    errh->warning("output %d matches no packets", i);
}

String
Program::unparse() const
{
    StringAccum sa;
    for (int i = 0; i < _insn.size(); i++) {
	Insn in = _insn[i];
	in.offset -= _align_offset;
	sa << (i < 10 ? " " : "") << i << ' ' << in << '\n';
    }
    if (_insn.size() == 0)
	sa << "all->[" << _output_everything << "]\n";
    sa << "safe length " << _safe_length << "\n";
    sa << "alignment offset " << _align_offset << "\n";
    return sa.take_string();
}


//
// COMPRESSION
//

void
CompressedProgram::compile(const Program &prog, bool perform_binary_search,
			   unsigned min_binary_search)
{
    // Compress the program into "zprog."

    // The compressed program groups related instructions together and sorts
    // large sequences of common primitives ("port 80 or port 90 or port 92 or
    // ..."), allowing the use of binary search.

    // The compressed program is a sequence of tests.  Each test consists of
    // five or more 32-bit words, as follows.
    //
    // +----------+--------+--------+--------+--------+-------
    // |nval|S|off|   no   |   yes  |  mask  |  value | value...
    // +----------+--------+--------+--------+--------+-------
    // nval (15 bits)  - number of values in the test
    // S (1 bit)       - short output
    // off (16 bits)   - offset of word into the data packet
    //                   (might be > TRANSP_FAKE_OFFSET)
    // no (32 bits)    - jump if test fails
    // yes (32 bits)   - jump if test succeeds
    // mask (32 bits)  - masked with packet data before comparing with values
    // value (32 bits) - comparison data (nval values).  The values are sorted
    //                   in numerical order if 'nval >= min_binary_search.'
    //
    // The test succeeds if the 32 bits of packet data starting at 'off,'
    // bitwise anded with 'mask,' equal any one of the 'value's.  If a 'jump'
    // value is <= 0, it is the negative of the relevant IPFilter output port.
    // A positive 'jump' value equals the number of 32-bit words to move the
    // instruction pointer.

    // It often helps to do another bubblesort for things like ports.

    _zprog.clear();
    _output_everything = prog.output_everything();
    _safe_length = prog.safe_length();
    _align_offset = prog.align_offset();

    Vector<int> wanted(prog.ninsn() + 1, 0);
    wanted[0] = 1;
    for (const Insn *in = prog.begin(); in != prog.end(); ++in)
	if (wanted[in - prog.begin()])
	    for (int j = 0; j < 2; j++)
		if (in->j[j] > 0)
		    wanted[in->j[j]]++;

    Vector<int> offsets;
    for (int i = 0; i < prog.ninsn(); i++) {
	int off = _zprog.size();
	offsets.push_back(off);
	if (wanted[i] == 0)
	    continue;
	const Insn &in = prog.insn(i);
	_zprog.push_back(in.offset + (in.short_output ? 0x10000 : 0) + 0x20000);
	_zprog.push_back(in.no());
	_zprog.push_back(in.yes());
	_zprog.push_back(in.mask.u);
	_zprog.push_back(in.value.u);
	int no;
	while ((no = (int32_t) _zprog[off+1]) > 0 && wanted[no] == 1
	       && prog.insn(no).yes() == in.yes()
	       && prog.insn(no).offset == in.offset
	       && prog.insn(no).mask.u == in.mask.u) {
	    _zprog[off] += 0x20000;
	    _zprog[off+1] = prog.insn(no).no();
	    _zprog.push_back(prog.insn(no).value.u);
	    wanted[no]--;
	}
	if (perform_binary_search && (_zprog[off] >> 17) >= min_binary_search)
	    click_qsort(&_zprog[off+4], _zprog[off] >> 17);
    }
    offsets.push_back(_zprog.size());

    for (int i = 0; i < prog.ninsn(); i++)
	if (offsets[i] < _zprog.size() && offsets[i] < offsets[i+1]) {
	    int off = offsets[i];
	    if ((int32_t) _zprog[off+1] > 0)
		_zprog[off+1] = offsets[_zprog[off+1]] - off;
	    if ((int32_t) _zprog[off+2] > 0)
		_zprog[off+2] = offsets[_zprog[off+2]] - off;
	}
}

void
CompressedProgram::warn_unused_outputs(int noutputs, ErrorHandler *errh) const
{
    Vector<int> used(noutputs, 0);
    if (_output_everything >= 0 && _output_everything < noutputs)
	used[_output_everything] = 1;
    else
	for (int i = 0; i < _zprog.size(); ) {
	    for (int k = 1; k < 3; ++k) {
		int32_t output = _zprog[i+k];
		if (output <= 0 && -output < noutputs)
		    used[-output] = 1;
	    }
	    i += 4 + (_zprog[i] >> 17);
	}

    for (int i = 0; i < noutputs; ++i)
	if (!used[i])
	    errh->warning("output %d matches no packets", i);
}

String
CompressedProgram::unparse() const
{
    Vector<int> stepno(_zprog.size(), 0);
    for (int i = 0; i < _zprog.size(); ) {
	int nparts = _zprog[i] >> 17;
	if (i + 4 + nparts < _zprog.size())
	    stepno[i + 4 + nparts] = stepno[i] + nparts;
	i += 4 + nparts;
    }

    StringAccum sa;
    for (int i = 0; i < _zprog.size(); ) {
	int nparts = _zprog[i] >> 17;
	for (int j = 0; j < nparts; ++j) {
	    int mystep = stepno[i] + j;
	    int32_t no, yes;
	    if (j + 1 < nparts)
		no = mystep + 1;
	    else if ((no = _zprog[i + 1]) > 0)
		no = stepno[i + no];
	    if ((yes = _zprog[i + 2]) > 0)
		yes = stepno[i + yes];
	    Insn in((uint16_t) _zprog[i] - _align_offset,
		    _zprog[i + 4 + j], _zprog[i + 3], no, yes,
		    _zprog[i] & 0x10000);
	    sa << (mystep < 10 ? " " : "") << mystep << ' ' << in << '\n';
	}
	i += 4 + nparts;
    }
    if (_zprog.size() == 0)
	sa << "all->[" << _output_everything << "]\n";
    sa << "safe length " << _safe_length << "\n";
    sa << "alignment offset " << _align_offset << "\n";
    return sa.take_string();
}


//
// RUNNING
//

int
Program::length_checked_match(const Packet *p)
{
    const unsigned char *packet_data = p->data() - _align_offset;
    int packet_length = p->length() + _align_offset; // XXX >= MAXINT?
    Insn *ex = &_insn[0];	// avoid bounds checking
    int pos = 0;
    uint32_t data;

    do {
	int offset = ex[pos].offset;
	if (offset + Insn::width > packet_length)
	    goto check_length;

    length_ok:
	data = *(const uint32_t *)(packet_data + offset);
	data &= ex[pos].mask.u;
	pos = ex[pos].j[data == ex[pos].value.u];
	continue;

    check_length:
	if (offset < packet_length) {
	    unsigned available = packet_length - offset;
	    if (!(ex[pos].mask.c[3]
		  || (ex[pos].mask.c[2] && available <= 2)
		  || (ex[pos].mask.c[1] && available == 1)))
		goto length_ok;
	}
	pos = ex[pos].j[ex[pos].short_output];
    } while (pos > 0);

    return -pos;
}

}}
CLICK_ENDDECLS
ELEMENT_PROVIDES(Classification)
