// -*- c-basic-offset: 4 -*-
/*
 * ratedsplitter.{cc,hh} -- split packets at a given rate.
 * Benjie Chen, Eddie Kohler
 * 
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
#include "ratedsplitter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
CLICK_DECLS

RatedSplitter::RatedSplitter()
    : Element(1, 2)
{
    MOD_INC_USE_COUNT;
}

RatedSplitter::~RatedSplitter()
{
    MOD_DEC_USE_COUNT;
}

int
RatedSplitter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t r;
    CpVaParseCmd cmd = (is_bandwidth() ? cpBandwidth : cpUnsigned);
    if (cp_va_parse(conf, this, errh, 
		    cmd, "split rate", &r, 0) < 0) 
	return -1;
    _rate.set_rate(r, errh);
    return 0;
}

void
RatedSplitter::configuration(Vector<String> &conf) const
{
    conf.push_back(String(_rate.rate()));
}

void
RatedSplitter::push(int, Packet *p)
{
    struct timeval now;
    click_gettimeofday(&now);
    if (_rate.need_update(now)) {
	_rate.update();
	output(0).push(p);
    } else
	output(1).push(p);
}


// HANDLERS

void
RatedSplitter::add_handlers()
{
    add_read_handler("rate", read_positional_handler, (void *)0);
    add_write_handler("rate", reconfigure_positional_handler, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RatedSplitter)
