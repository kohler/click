// -*- c-basic-offset: 4; related-file-name: "../include/click/bitvector.hh" -*-
/*
 * bitvector.{cc,hh} -- generic bit vector class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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

#include <click/bitvector.hh>

void
Bitvector::finish_copy_constructor(const Bitvector &o)
{
    int nn = u_max();
    _data = new uint32_t[nn + 1];
    for (int i = 0; i <= nn; i++)
	_data[i] = o._data[i];
}

void
Bitvector::clear()
{
    int nn = u_max();
    for (int i = 0; i <= nn; i++)
	_data[i] = 0;
}

bool
Bitvector::zero() const
{
    int nn = u_max();
    for (int i = 0; i <= nn; i++)
	if (_data[i])
	    return false;
    return true;
}

void
Bitvector::resize_x(int n, bool valid_n)
{
    int want_u = ((n-1)/32) + 1;
    int have_u = (valid_n ? u_max() + 1 : INLINE_UNSIGNEDS);
    if (have_u < INLINE_UNSIGNEDS)
	have_u = INLINE_UNSIGNEDS;
    if (want_u <= have_u)
	return;

    uint32_t *new_data = new uint32_t[want_u];
    for (int i = 0; i < have_u; i++)
	new_data[i] = _data[i];
    for (int i = have_u; i < want_u; i++)
	new_data[i] = 0;
    if (valid_n && _max >= INLINE_BITS)
	delete[] _data;
    _data = new_data;
}

void
Bitvector::clear_last()
{
    if ((_max&0x1F) != 0x1F) {
	uint32_t mask = (1U << ((_max&0x1F)+1)) - 1;
	_data[_max>>5] &= mask;
    } else if (_max < 0)
	_data[0] = 0;
}

Bitvector &
Bitvector::operator=(const Bitvector &o)
{
    if (&o != this) {
	resize(o._max + 1);
	int copy = u_max();
	if (copy < INLINE_UNSIGNEDS - 1)
	    copy = INLINE_UNSIGNEDS - 1;
	for (int i = 0; i <= copy; i++)
	    _data[i] = o._data[i];
    }
    return *this;
}

Bitvector &
Bitvector::assign(int n, bool value)
{
    resize(n);
    uint32_t bits = (value ? 0xFFFFFFFFU : 0U);
    int copy = u_max();
    if (copy < INLINE_UNSIGNEDS - 1)
	copy = INLINE_UNSIGNEDS - 1;
    for (int i = 0; i <= copy; i++)
	_data[i] = bits;
    if (value)
	clear_last();
    return *this;
}

void
Bitvector::negate()
{
    int nn = u_max();
    uint32_t *data = _data;
    for (int i = 0; i <= nn; i++)
	data[i] = ~data[i];
    clear_last();
}

Bitvector &
Bitvector::operator&=(const Bitvector &o)
{
    assert(o._max == _max);
    int nn = u_max();
    uint32_t *data = _data, *o_data = o._data;
    for (int i = 0; i <= nn; i++)
	data[i] &= o_data[i];
    return *this;
}

Bitvector &
Bitvector::operator|=(const Bitvector &o)
{
    if (o._max > _max)
	resize(o._max + 1);
    int nn = u_max();
    uint32_t *data = _data, *o_data = o._data;
    for (int i = 0; i <= nn; i++)
	data[i] |= o_data[i];
    return *this;
}

Bitvector &
Bitvector::operator^=(const Bitvector &o)
{
    assert(o._max == _max);
    int nn = u_max();
    uint32_t *data = _data, *o_data = o._data;
    for (int i = 0; i <= nn; i++)
	data[i] ^= o_data[i];
    return *this;
}

void
Bitvector::or_at(const Bitvector &o, int offset)
{
    assert(offset >= 0 && offset + o._max <= _max);
    uint32_t bits_1st = offset&0x1F;
    int my_pos = offset>>5;
    int o_pos = 0;
    int my_u_max = u_max();
    int o_u_max = o.u_max();
    uint32_t *data = _data;
    uint32_t *o_data = o._data;

    while (true) {
	uint32_t val = o_data[o_pos];
	data[my_pos] |= (val << bits_1st);

	my_pos++;
	if (my_pos > my_u_max)
	    break;
    
	if (bits_1st)
	    data[my_pos] |= (val >> (32 - bits_1st));
    
	o_pos++;
	if (o_pos > o_u_max)
	    break;
    }
}

void
Bitvector::or_with_difference(const Bitvector &o, Bitvector &diff)
{
    if (o._max > _max)
	resize(o._max + 1);
    if (diff._max > _max)
	diff.resize(o._max + 1);
    int nn = u_max();
    uint32_t *data = _data, *diff_data = diff._data;
    const uint32_t *o_data = o._data;
    for (int i = 0; i <= nn; i++) {
	diff_data[i] = o_data[i] & ~data[i];
	data[i] |= o_data[i];
    }
}

bool
Bitvector::nonzero_intersection(const Bitvector &o) const
{
    int nn = o.u_max();
    if (nn > u_max())
	nn = u_max();
    const uint32_t *data = _data, *o_data = o._data;
    for (int i = 0; i <= nn; i++)
	if (data[i] & o_data[i])
	    return true;
    return false;
}
