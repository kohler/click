// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * shaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "shaper.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Shaper::Shaper()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

Shaper::~Shaper()
{
    MOD_DEC_USE_COUNT;
}

Shaper *
Shaper::clone() const
{
    return new Shaper;
}

int
Shaper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned rate;
    if (cp_va_parse(conf, this, errh,
		    cpUnsigned, "max allowable rate", &rate,
		    0) < 0)
	return -1;
    _rate.set_rate(rate, errh);
    return 0;
}

Packet *
Shaper::pull(int)
{
    Packet *p = 0;
    struct timeval now;
    click_gettimeofday(&now);
    if (_rate.need_update(now)) {
	if ((p = input(0).pull()))
	    _rate.update();
    }
    return p;
}

void
Shaper::add_handlers()
{
    add_read_handler("rate", read_positional_handler, (void *)0);
    add_write_handler("rate", reconfigure_positional_handler, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Shaper)
