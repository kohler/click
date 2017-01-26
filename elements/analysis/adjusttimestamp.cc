/*
 * adjusttimestamp.{cc,hh} -- adjust timestamp annotations
 * Eddie Kohler
 *
 * Copyright (c) 2011 Regents of the University of California
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
#include "adjusttimestamp.hh"
#include <click/args.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
CLICK_DECLS

AdjustTimestamp::AdjustTimestamp()
{
}

AdjustTimestamp::~AdjustTimestamp()
{
}

int
AdjustTimestamp::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _first = _all = false;
    _ts.clear();
    return Args(conf, this, errh)
	.read_p("TIME", TimestampArg(true), _ts)
	.read("FIRST", _first)
	.read("ALL", _all).complete();
}

Packet *
AdjustTimestamp::simple_action(Packet *p)
{
    Timestamp *ts;
    if (!_first)
	ts = &p->timestamp_anno();
    else
	ts = &FIRST_TIMESTAMP_ANNO(p);
    if (*ts || _all)
	*ts += _ts;
    return p;
}

void
AdjustTimestamp::add_handlers()
{
    add_data_handlers("time", Handler::f_read | Handler::f_write, &_ts, true);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AdjustTimestamp)
