// -*- c-basic-offset: 4; related-file-name: "../include/click/integers.hh" -*-
/*
 * integers.{cc,hh} -- unaligned/network-order "integers"
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

#include <click/config.h>
#include <click/glue.hh>
#include <click/integers.hh>


// first_bit_set(uint32_t) borrowed from tcpdpriv

int
first_bit_set(uint32_t value)
{
    int add = 0;
    static uint8_t bvals[] = { 0, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 };

    if ((value & 0xFFFF0000) == 0) {
	if (value == 0)		/* zero input ==> zero output */
	    return 0;
	add += 16;
    } else
	value >>= 16;

    if ((value & 0xFF00) == 0)
	add += 8;
    else
	value >>= 8;

    if ((value & 0xF0) == 0)
	add += 4;
    else
	value >>= 4;

    return add + bvals[value & 0xf];
}


#ifdef HAVE_INT64_TYPES

int
first_bit_set(uint64_t value)
{
    uint32_t hi = (uint32_t)(value >> 32);
    if (hi == 0) {
	uint32_t lo = (uint32_t)value;
	return (lo ? 32 + first_bit_set(lo) : 0);
    } else
	return first_bit_set(hi);
}

#endif
