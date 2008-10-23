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

bool
Alignment::operator<=(const Alignment &o) const
{
    if (_modulus < 0 || o._modulus < 0)
	return false;
    else if (o._modulus <= 1)
	return true;
    else if (_modulus <= 1)
	return false;
    else if (_modulus % o._modulus != 0)
	return false;
    else
	return (_offset % o._modulus == o._offset);
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

Alignment &
Alignment::operator|=(const Alignment &o)
{
    if (_modulus == 0 || o._modulus < 0)
	return (*this = o);
    else if (_modulus <= 1 || o._modulus == 0)
	return *this;

    // new_modulus = gcd(_modulus, o._modulus)
    int new_modulus = _modulus, b = o._modulus;
    while (b) {			// Euclid's algorithm
	int r = new_modulus % b;
	new_modulus = b;
	b = r;
    }

    // calculate new offsets
    int new_off1 = _offset % new_modulus;
    int new_off2 = o._offset % new_modulus;
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
Alignment::operator&=(const Alignment &o)
{
    // XXX doesn't work for arbitrary alignments; should use some set method
    // and least-common-multiple
    if (empty() || o <= *this)
	return (*this = o);
    else if (*this <= o)
	return *this;
    else
	return (*this = Alignment(-1, 0, 0));
}

String
Alignment::unparse() const
{
    if (bad())
	return "BAD";
    else if (empty())
	return "EMPTY";
    else
	return String(_modulus) + "/" + String(_offset);
}
