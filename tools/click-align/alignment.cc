/*
 * alignment.{cc,hh} -- represents alignment constraints
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "alignment.hh"
#include <click/args.hh>
#include "elementt.hh"

Alignment::Alignment(ElementT *e)
{
    int modulus, offset;
    if (Args().push_back_args(e->configuration())
	.read_mp("MODULUS", modulus)
	.read_mp("OFFSET", offset)
	.execute() >= 0
	&& modulus > 0 && offset >= 0 && offset < modulus) {
	_modulus = modulus;
	_offset = offset;
    } else {
	_modulus = bad_modulus;
	_offset = 0;
    }
}

bool
Alignment::operator<=(const Alignment &x) const
{
    if (_modulus == bad_modulus || x._modulus == bad_modulus)
	return false;
    else if (_modulus == universal_modulus || x._modulus == universal_modulus)
	return _modulus == universal_modulus;
    else if (x._modulus <= 1)
	return true;
    else if (_modulus <= 1)
	return false;
    else if (_modulus % x._modulus != 0)
	return false;
    else
	return (_offset % x._modulus == x._offset);
}

Alignment &
Alignment::operator+=(int delta)
{
    if (_modulus > 1) {
	if (delta < 0)
	    delta += ((-delta)/_modulus + 1) * _modulus;
	_offset = (_offset + delta) % _modulus;
    }
    return *this;
}

static int
gcd(int a, int b)
{
    int x = a;
    while (b) {			// Euclid's algorithm
	int r = x % b;
	x = b;
	b = r;
    }
    return x;
}

Alignment &
Alignment::operator|=(const Alignment &x)
{
    if (_modulus == empty_modulus || x._modulus == bad_modulus
        || (_modulus == universal_modulus && x._modulus > 0))
	return (*this = x);
    else if (_modulus <= 1 || x._modulus <= 0)
	return *this;

    // new_modulus = gcd(_modulus, x._modulus)
    int new_modulus = gcd(_modulus, x._modulus);

    // calculate new offsets
    int new_off1 = _offset % new_modulus;
    int new_off2 = x._offset % new_modulus;
    if (new_off1 == new_off2) {
	_modulus = new_modulus;
	_offset = new_off1;
	return *this;
    }

    // check for lowering modulus
    int diff = new_off2 - new_off1;
    if (diff < 0)
	diff += new_modulus;
    if (diff > new_modulus / 2)
	diff = new_modulus - diff;
    if (new_modulus % diff == 0) {
	_modulus = diff;
	_offset = new_off1 % diff;
    } else {
	_modulus = 1;
	_offset = 0;
    }

    return *this;
}

Alignment &
Alignment::operator&=(const Alignment &x)
{
    assert(_modulus != universal_modulus && x._modulus != universal_modulus);

    if (_modulus <= 0 || x._modulus <= 0) {
	if (_modulus == empty_modulus || x._modulus == bad_modulus)
	    return (*this = x);
	else
	    return *this;
    }

    if (*this <= x)
	return *this;
    else if (x <= *this)
	return (*this = x);
    else if (_modulus == x._modulus) // fast fail
	return (*this = make_bad());

    int lcm = (_modulus * x._modulus) / gcd(_modulus, x._modulus);
    const Alignment *smaller = (_modulus < x._modulus ? this : &x);
    const Alignment *larger = (_modulus < x._modulus ? &x : this);
    int otry = larger->_offset;
    while (otry < lcm && otry % smaller->_modulus != smaller->_offset)
	otry += larger->_modulus;

    if (otry < lcm)
	return (*this = Alignment(lcm, otry, 0));
    else
	return (*this = make_bad());
}

String
Alignment::unparse() const
{
    if (bad())
	return String::make_stable("BAD", 3);
    else if (empty())
	return String::make_stable("EMPTY", 5);
    else if (universal())
	return String::make_stable("UNIVERSAL", 9);
    else
	return String(_modulus) + "/" + String(_offset);
}

#if 0
void
Alignment::test()
{
    assert(Alignment(1, 0) <= Alignment());
    assert(Alignment() <= Alignment(1, 0));
    assert(!(Alignment(2, 0) <= Alignment(4, 0)));
    assert(Alignment(4, 0) <= Alignment(2, 0));
    assert(Alignment(4, 1) <= Alignment(2, 1));
    assert(Alignment::make_universal() <= Alignment(2, 1));
    assert((Alignment(4, 1) & Alignment(2, 1)) == Alignment(4, 1));
    assert((Alignment(4, 1) | Alignment(2, 1)) == Alignment(2, 1));
    assert((Alignment(4, 1) & Alignment(5, 1)) == Alignment(20, 1));
    assert((Alignment(4, 1) & Alignment(5, 3)) == Alignment(20, 13));
    assert((Alignment(6, 1) & Alignment(10, 7)) == Alignment(30, 7));
    assert((Alignment(4, 1) & Alignment(4, 2)).bad());
}
#endif
