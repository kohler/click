// -*- c-basic-offset: 4; related-file-name: "../include/click/timestamp.hh" -*-
/*
 * timestamp.{cc,hh} -- timestamps
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include <click/timestamp.hh>
#include <click/straccum.hh>
CLICK_DECLS

StringAccum &
operator<<(StringAccum &sa, const struct timeval &tv)
{
    if (char *x = sa.reserve(30)) {
	int len;
	if (tv.tv_sec >= 0)
	    len = sprintf(x, "%ld.%06ld", (long)tv.tv_sec, (long)tv.tv_usec);
	else
	    len = sprintf(x, "-%ld.%06ld", -((long)tv.tv_sec) - 1L, 1000000L - (long)tv.tv_usec);
	sa.forward(len);
    }
    return sa;
}

StringAccum &
operator<<(StringAccum &sa, const Timestamp& ts)
{
    if (char *x = sa.reserve(33)) {
	uint32_t sec, subsec;
	if (ts.sec() >= 0)
	    sec = ts.sec(), subsec = ts.subsec();
	else {
	    *x++ = '-', sa.forward(1);
	    sec = -ts.sec() - 1, subsec = Timestamp::SUBSEC_PER_SEC - ts.subsec();
	}
	
	int len;
#if HAVE_NANOTIMESTAMP
	uint32_t usec = subsec / 1000;
	if (usec * 1000 == subsec)
	    len = sprintf(x, "%u.%06u", sec, usec);
	else
	    len = sprintf(x, "%u.%09u", sec, subsec);
#else
	len = sprintf(x, "%u.%06u", sec, subsec);
#endif
	sa.forward(len);
    }
    return sa;
}

String
Timestamp::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

CLICK_ENDDECLS
