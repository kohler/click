/*
 * vector.{cc,hh} -- simple array template class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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

#ifndef CLICK_VECTOR_CC
#define CLICK_VECTOR_CC
#include <click/glue.hh>
#include <click/vector.hh>
CLICK_DECLS
/** @cond never */

template <typename AM>
vector_memory<AM>::~vector_memory()
{
    AM::destroy(l_, n_);
    CLICK_LFREE(l_, capacity_ * sizeof(type));
}

template <typename AM>
void vector_memory<AM>::assign(const vector_memory<AM> &x)
{
    if (&x != this) {
	AM::destroy(l_, n_);
	AM::mark_noaccess(l_, n_);
	n_ = 0;
	if (reserve_and_push_back(x.n_, 0)) {
	    n_ = x.n_;
	    AM::mark_undefined(l_, n_);
	    AM::copy(l_, x.l_, n_);
	}
    }
}

template <typename AM>
void vector_memory<AM>::assign(size_type n, const type *vp)
{
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return assign(n, &v_copy);
    }

    resize(0, vp);
    resize(n, vp);
}

template <typename AM>
typename vector_memory<AM>::iterator vector_memory<AM>::insert(iterator it, const type *vp)
{
    assert(it >= begin() && it <= end());
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return insert(it, &v_copy);
    }

    if (n_ == capacity_) {
	size_type pos = it - begin();
	if (!reserve_and_push_back(-1, 0))
	    return end();
	it = begin() + pos;
    }
    AM::mark_undefined(l_ + n_, 1);
    AM::move(it + 1, it, end() - it);
    AM::mark_undefined(it, 1);
    AM::fill(it, 1, vp);
    ++n_;
    return it;
}

template <typename AM>
typename vector_memory<AM>::iterator vector_memory<AM>::erase(iterator a, iterator b)
{
    if (a < b) {
	assert(a >= begin() && b <= end());
	AM::move_onto(a, b, end() - b);
	n_ -= b - a;
	AM::destroy(end(), b - a);
	AM::mark_noaccess(end(), b - a);
	return a;
    } else
	return b;
}

template <typename AM>
bool vector_memory<AM>::reserve_and_push_back(size_type want, const type *push_vp)
{
    if (unlikely(push_vp && need_argument_copy(push_vp))) {
	type push_v_copy(*push_vp);
	return reserve_and_push_back(want, &push_v_copy);
    }

    if (want < 0)
	want = (capacity_ > 0 ? capacity_ * 2 : 4);

    if (want > capacity_) {
	type *new_l = (type *) CLICK_LALLOC(want * sizeof(type));
	if (!new_l)
	    return false;
	AM::mark_noaccess(new_l + n_, want - n_);
	AM::move(new_l, l_, n_);
	CLICK_LFREE(l_, capacity_ * sizeof(type));
	l_ = new_l;
	capacity_ = want;
    }

    if (unlikely(push_vp))
	push_back(push_vp);
    return true;
}

template <typename AM>
void vector_memory<AM>::resize(size_type n, const type *vp)
{
    if (unlikely(need_argument_copy(vp))) {
	type v_copy(*vp);
	return resize(n, &v_copy);
    }

    if (n <= capacity_ || reserve_and_push_back(n, 0)) {
	assert(n >= 0);
	if (n < n_) {
	    AM::destroy(l_ + n, n_ - n);
	    AM::mark_noaccess(l_ + n, n_ - n);
	}
	if (n_ < n) {
	    AM::mark_undefined(l_ + n_, n - n_);
	    AM::fill(l_ + n_, n - n_, vp);
	}
	n_ = n;
    }
}

template <typename AM>
void vector_memory<AM>::swap(vector_memory<AM> &x)
{
    type *l = l_;
    l_ = x.l_;
    x.l_ = l;

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
