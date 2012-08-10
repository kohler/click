// -*- c-basic-offset: 4; related-file-name: "../include/click/bitvector.hh" -*-
/*
 * bitvector.{cc,hh} -- generic bit vector class
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
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
CLICK_DECLS

void
Bitvector::finish_copy_constructor(const Bitvector &o)
{
    int nn = max_word();
    _data = new word_type[nn + 1];
    for (int i = 0; i <= nn; i++)
	_data[i] = o._data[i];
}

/** @brief Test if the bitvector's bits are all false. */
bool
Bitvector::zero() const
{
    int nn = max_word();
    for (int i = 0; i <= nn; i++)
	if (_data[i])
	    return false;
    return true;
}

/** @brief Set all bits to false. */
void
Bitvector::clear()
{
    int nn = max_word();
    for (int i = 0; i <= nn; i++)
	_data[i] = 0;
}

void
Bitvector::resize_to_max(int new_max, bool valid_n)
{
    int want_u = (new_max >> wshift) + 1;
    int have_u = (valid_n ? max_word() : MAX_INLINE_WORD) + 1;
    if (have_u < MAX_INLINE_WORD + 1)
	have_u = MAX_INLINE_WORD + 1;
    if (want_u <= have_u)
	return;

    word_type *new_data = new word_type[want_u];
    memcpy(new_data, _data, have_u * sizeof(word_type));
    memset(new_data + have_u, 0, (want_u - have_u) * sizeof(word_type));
    if (_data != &_f0)
	delete[] _data;
    _data = new_data;
}

void
Bitvector::clear_last()
{
    if (unlikely(_max < 0))
	_data[0] = 0;
    else if ((_max & wmask) != wmask) {
	word_type mask = (word_type(1) << ((_max & wmask) + 1)) - 1;
	_data[_max>>wshift] &= mask;
    }
}

/** @brief Set this bitvector to a copy of @a x.
    @return *this */
Bitvector &
Bitvector::operator=(const Bitvector &x)
{
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
    // We might not have been initialized properly.
    if (!_data)
	_max = -1, _data = &_f0;
#endif
    if (&x == this)
	/* nada */;
    else if (x.max_word() <= MAX_INLINE_WORD)
	memcpy(_data, x._data, (MAX_INLINE_WORD + 1) * sizeof(word_type));
    else {
	if (_data != &_f0)
	    delete[] _data;
	_data = new word_type[x.word_size()];
	memcpy(_data, x._data, x.word_size() * sizeof(word_type));
    }
    _max = x._max;
    return *this;
}

/** @brief Set the bitvector to @a bit-valued length-@a n.
    @pre @a n >= 0
    @return *this */
Bitvector &
Bitvector::assign(int n, bool bit)
{
    resize(n);
    word_type bits = (bit ? ~word_type(0) : word_type(0));
    // 24.Jun.2008 -- Even if n <= 0, at least one word must be set to "bits."
    // Otherwise assert(_max >= 0 || _data[0] == 0) will not hold.
    int copy = (n > 0 ? word_size() : 1);
    for (int i = 0; i < copy; i++)
	_data[i] = bits;
    if (bit)
	clear_last();
    return *this;
}

/** @brief Flip all bits in this bitvector. */
void
Bitvector::flip()
{
    int nn = max_word();
    word_type *data = _data;
    for (int i = 0; i <= nn; i++)
	data[i] = ~data[i];
    clear_last();
}

/** @brief Modify this bitvector by bitwise and with @a x.
    @pre @a x.size() == size()
    @return *this */
Bitvector &
Bitvector::operator&=(const Bitvector &x)
{
    assert(x._max == _max);
    int nn = max_word();
    word_type *data = _data, *x_data = x._data;
    for (int i = 0; i <= nn; i++)
	data[i] &= x_data[i];
    return *this;
}

/** @brief Modify this bitvector by bitwise or with @a x.
    @post new size() == max(old size(), x.size())
    @return *this */
Bitvector &
Bitvector::operator|=(const Bitvector &x)
{
    if (x._max > _max)
	resize(x._max + 1);
    int nn = max_word();
    nn = (nn < x.max_word() ? nn : x.max_word());
    word_type *data = _data, *x_data = x._data;
    for (int i = 0; i <= nn; i++)
	data[i] |= x_data[i];
    return *this;
}

/** @brief Modify this bitvector by bitwise exclusive or with @a x.
    @pre @a x.size() == size()
    @return *this */
Bitvector &
Bitvector::operator^=(const Bitvector &x)
{
    assert(x._max == _max);
    int nn = max_word();
    word_type *data = _data, *x_data = x._data;
    for (int i = 0; i <= nn; i++)
	data[i] ^= x_data[i];
    return *this;
}

/** @brief Modify this bitvector by bitwise or with an offset @a x.
    @param x bitwise or operand
    @param offset initial offset
    @pre @a offset >= 0 && @a offset + @a x.size() <= size()

    Logically shifts @a x to start at position @a offset, then performs
    a bitwise or.  <code>a.offset_or(b, offset)</code> is equivalent to:
    @code
    for (int i = 0; i < b.size(); ++i)
        a[offset+i] |= b[i];
     @endcode */
void
Bitvector::offset_or(const Bitvector &x, int offset)
{
    assert(offset >= 0 && offset + x._max <= _max);
    int bits_1st = offset & wmask;
    int my_pos = offset >> wshift;
    int x_pos = 0;
    int my_max_word = max_word();
    int x_max_word = x.max_word();
    word_type *data = _data;
    const word_type *x_data = x._data;
    assert((x._max < 0 && x_data[0] == 0)
	   || (x._max & wmask) == wmask
	   || (x_data[x_max_word] & ((word_type(1) << ((x._max & wmask) + 1)) - 1)) == x_data[x_max_word]);

    while (true) {
	word_type val = x_data[x_pos];
	data[my_pos] |= (val << bits_1st);

	my_pos++;
	if (my_pos > my_max_word)
	    break;

	if (bits_1st)
	    data[my_pos] |= (val >> (wbits - bits_1st));

	x_pos++;
	if (x_pos > x_max_word)
	    break;
    }
}


/** @brief Modify this bitvector by bitwise or, returning difference.
    @param x bitwise or operand
    @param[out] difference set to (@a x - old *this)
    @pre @a x.size() == size()
    @post @a difference.size() == size()
    @post @a x | *this == *this
    @post (@a difference & *this & @a x) == @a difference

    Same as operator|=, but any newly set bits are returned in @a
    difference. */
void
Bitvector::or_with_difference(const Bitvector &x, Bitvector &difference)
{
    assert(x._max == _max);
    if (difference._max != _max)
	difference.resize(_max + 1);
    int nn = max_word();
    word_type *data = _data, *diff_data = difference._data;
    const word_type *x_data = x._data;
    for (int i = 0; i <= nn; i++) {
	diff_data[i] = x_data[i] & ~data[i];
	data[i] |= x_data[i];
    }
}

/** @brief Test whether this bitvector and @a x have a common true bit.

    This bitvector and @a x may have different sizes. */
bool
Bitvector::nonzero_intersection(const Bitvector &x) const
{
    int nn = x.word_size();
    if (nn > word_size())
	nn = word_size();
    const word_type *data = _data, *x_data = x._data;
    for (int i = 0; i < nn; i++)
	if (data[i] & x_data[i])
	    return true;
    return false;
}

/** @brief Swap the contents of this bitvector and @a x. */
void
Bitvector::swap(Bitvector &x)
{
    word_type u = _f0;
    _f0 = x._f0;
    x._f0 = u;

    u = _f1;
    _f1 = x._f1;
    x._f1 = u;

    int m = _max;
    _max = x._max;
    x._max = m;

    word_type *d = _data;
    _data = (x._data == &x._f0 ? &_f0 : x._data);
    x._data = (d == &_f0 ? &x._f0 : d);
}

CLICK_ENDDECLS
