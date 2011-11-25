/*
 * deque.{cc,hh} -- double-ended queue template class
 * Eddie Kohler, Douglas S. J. De Couto
 * Based on code from Click Vector<> class (vector.{cc,hh}).
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2011 Eddie Kohler
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

#ifndef CLICK_DEQUE_CC
#define CLICK_DEQUE_CC
#include <click/deque.hh>
CLICK_DECLS
/** @cond never */

template <typename AM>
deque_memory<AM>::~deque_memory()
{
    size_type f = naccess(n_);
    AM::destroy(l_ + head_, f);
    AM::destroy(l_, n_ - f);
    CLICK_LFREE(l_, capacity_ * sizeof(type));
}

template <typename AM>
void deque_memory<AM>::assign(const deque_memory<AM> &x)
{
    if (&x != this) {
	size_type f = naccess(n_);
	AM::destroy(l_ + head_, f);
	AM::destroy(l_, n_ - f);
	n_ = head_ = 0;
	if (reserve_and_push(x.n_, false, 0)) {
	    n_ = x.n_;
	    AM::mark_undefined(l_, n_);
	    f = x.naccess(n_);
	    AM::copy(l_, x.l_ + x.head_, f);
	    AM::copy(l_ + f, x.l_, n_ - f);
	}
    }
}

template <typename AM>
void deque_memory<AM>::assign(size_type n, const type *vp)
{
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return assign(n, &v_copy);
    }

    resize(0, vp);
    resize(n, vp);
}

template <typename AM>
bool deque_memory<AM>::insert(size_type i, const type *vp)
{
    assert((unsigned) i <= (unsigned) n_);
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return insert(i, &v_copy);
    }

    if (n_ == capacity_ && !reserve_and_push(-1, false, 0))
	return false;
    size_type srcpp, dstpp, size;
    int delta;
    if (i <= n_ - i) {
	head_ = prevp(head_);
	srcpp = head_ + 1;
	dstpp = head_;
	size = i;
	delta = 1;
    } else {
	srcpp = head_ + n_ - 1;
	dstpp = head_ + n_;
	size = n_ - i;
	delta = -1;
    }
    while (size) {
	AM::mark_undefined(l_ + canonp(dstpp), 1);
	AM::move(l_ + canonp(dstpp), l_ + canonp(srcpp), 1);
	dstpp += delta, srcpp += delta, --size;
    }
    AM::mark_undefined(l_ + canonp(dstpp), 1);
    AM::fill(l_ + canonp(dstpp), 1, vp);
    ++n_;
    return true;
}

template <typename AM>
int deque_memory<AM>::erase(size_type ai, size_type bi)
{
    assert(ai >= bi || (ai >= 0 && (unsigned) bi <= (unsigned) n_));
    if (ai < bi) {
	size_type srcpp, dstpp, size;
	int delta;
	if (ai < n_ - bi) {
	    srcpp = head_ + ai - 1;
	    dstpp = head_ + bi - 1;
	    size = ai;
	    delta = -1;
	    head_ = canonp(head_ + bi - ai);
	} else {
	    srcpp = head_ + bi;
	    dstpp = head_ + ai;
	    size = n_ - bi;
	    delta = 1;
	}
	while (size) {
	    AM::destroy(l_ + canonp(dstpp), 1);
	    AM::copy(l_ + canonp(dstpp), l_ + canonp(srcpp), 1);
	    dstpp += delta, srcpp += delta, --size;
	}
	for (size = bi - ai; size; --size, dstpp += delta) {
	    AM::destroy(l_ + canonp(dstpp), 1);
	    AM::mark_noaccess(l_ + canonp(dstpp), 1);
	}
	n_ -= bi - ai;
	return ai;
    } else
	return bi;
}

template <typename AM>
bool deque_memory<AM>::reserve_and_push(size_type want, bool isfront, const type *push_vp)
{
    if (unlikely(push_vp && need_argument_copy(push_vp))) {
	type push_v_copy(*push_vp);
	return reserve_and_push(want, isfront, &push_v_copy);
    }

    if (want < 0)
	want = (capacity_ > 0 ? capacity_ * 2 : 4);

    if (want > capacity_) {
	type *new_l = (type *) CLICK_LALLOC(want * sizeof(type));
	if (!new_l)
	    return false;
	AM::mark_noaccess(new_l + n_, want - n_);
	size_type f = naccess(n_);
	AM::move(new_l, l_ + head_, f);
	AM::move(new_l + f, l_, n_ - f);
	CLICK_LFREE(l_, capacity_ * sizeof(type));
	l_ = new_l;
	head_ = 0;
	capacity_ = want;
    }

    if (unlikely(push_vp))
	(isfront ? push_front(push_vp) : push_back(push_vp));
    return true;
}

template <typename AM>
void deque_memory<AM>::resize(size_type n, const type *vp)
{
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return resize(n, &v_copy);
    }

    if (n <= capacity_ || reserve_and_push(n, false, 0)) {
	assert(n >= 0);
	if (n < n_)
	    for (size_type p = i2p(n), x = i2p(n_); p != x; p = nextp(p)) {
		AM::destroy(l_ + p, 1);
		AM::mark_noaccess(l_ + p, 1);
	    }
	if (n_ < n)
	    for (size_type p = i2p(n_), x = i2p(n); p != x; p = nextp(p)) {
		AM::mark_undefined(l_ + p, 1);
		AM::fill(l_ + p, 1, vp);
	    }
	n_ = n;
    }
}

template <typename AM>
void deque_memory<AM>::swap(deque_memory<AM> &x)
{
    type *l = l_;
    l_ = x.l_;
    x.l_ = l;

    size_type head = head_;
    head_ = x.head_;
    x.head_ = head;

    size_type n = n_;
    n_ = x.n_;
    x.n_ = n;

    size_type capacity = capacity_;
    capacity_ = x.capacity_;
    x.capacity_ = capacity;
}

/** @endcond never */
CLICK_ENDDECLS
#endif
