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
CLICK_DECLS

// ffs_msb(uint32_t) borrowed from tcpdpriv

#if !HAVE___BUILTIN_CLZ
int
ffs_msb(uint32_t value)
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

    return add + bvals[value & 0xF];
}
#endif

#if HAVE_INT64_TYPES && !(HAVE___BUILTIN_CLZLL && SIZEOF_LONG_LONG == 8) && !(HAVE___BUILTIN_CLZL && SIZEOF_LONG == 8) && !(HAVE___BUILTIN_CLZ && SIZEOF_INT == 8)
int
ffs_msb(uint64_t value)
{
    uint32_t hi = (uint32_t)(value >> 32);
    if (hi == 0) {
	uint32_t lo = (uint32_t)value;
	return (lo ? 32 + ffs_msb(lo) : 0);
    } else
	return ffs_msb(hi);
}
#endif


#if !HAVE___BUILTIN_FFS && !HAVE_FFS
int
ffs_lsb(uint32_t value)
{
    int add = 0;
    static uint8_t bvals[] = { 0, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1 };

    if ((value & 0x0000FFFF) == 0) {
	if (value == 0)		/* zero input ==> zero output */
	    return 0;
	add += 16;
	value >>= 16;
    }

    if ((value & 0x00FF) == 0) {
	add += 8;
	value >>= 8;
    }

    if ((value & 0x0F) == 0) {
	add += 4;
	value >>= 4;
    }

    return add + bvals[value & 0xF];
}
#endif

#if HAVE_INT64_TYPES && !(HAVE___BUILTIN_FFSLL && SIZEOF_LONG_LONG == 8) && !(HAVE___BUILTIN_FFSL && SIZEOF_LONG == 8) && !(HAVE___BUILTIN_FFS && SIZEOF_INT == 8)
int
ffs_lsb(uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    if (lo == 0) {
	uint32_t hi = (uint32_t)(value >> 32);
	return (hi ? 32 + ffs_lsb(hi) : 0);
    } else
	return ffs_lsb(lo);
}
#endif


uint32_t
int_sqrt(uint32_t u)
{
    uint32_t prev = 0x7FFFFFFF;
    uint32_t work = u;
    if (work > 0)
	while (work < prev)
	    prev = work, work = (work + (u/work))/2;
    return work;
}

#if HAVE_INT64_TYPES && !CLICK_LINUXMODULE
// linuxmodule does not support uint64_t division 
uint64_t
int_sqrt(uint64_t u)
{
    uint64_t prev = ~((uint64_t)1 << 63);
    uint64_t work = u;
    if (work > 0)
	while (work < prev)
	    prev = work, work = (work + (u/work))/2;
    return work;
}
#endif

CLICK_ENDDECLS
