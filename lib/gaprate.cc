// -*- c-basic-offset: 4; related-file-name: "../include/click/gaprate.hh" -*-
/*
 * gaprate.{cc,hh} -- measure rates through moving gaps
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2008 Regents of the University of California
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
#include <click/gaprate.hh>
#include <click/error.hh>
CLICK_DECLS

void
GapRate::set_rate(unsigned r, ErrorHandler *errh)
{
    if (r > GapRate::MAX_RATE && errh)
	errh->error("rate too large; lowered to %u", GapRate::MAX_RATE);
    set_rate(r);
}

CLICK_ENDDECLS
