// -*- c-basic-offset: 4; related-file-name: "../include/click/ewma64.hh" -*-
/*
 * ewma64.{cc,hh} -- 64-bit Exponential Weighted Moving Averages
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#ifdef HAVE_INT64_TYPES
#include <click/ewma64.hh>
CLICK_DECLS

void
DirectEWMA64::update_zero_period(unsigned period)
{
    // XXX use table lookup
    if (period >= 100)
	_avg = 0;
    else {
	uint64_t comp = compensation();
	for (; period > 0; period--)
	    _avg += static_cast<int64_t>(-_avg + comp) >> stability_shift();
    }
}

CLICK_ENDDECLS
#endif
