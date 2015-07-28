// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * timerange.{cc,hh} -- element observes range of timestamps
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
#include <click/error.hh>
#include "timerange.hh"
#include <click/args.hh>
#include <click/straccum.hh>
CLICK_DECLS

TimeRange::TimeRange()
{
}

TimeRange::~TimeRange()
{
}

int
TimeRange::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _simple = false;
    if (Args(conf, this, errh)
	.read("SIMPLE", _simple)
	.complete() < 0)
	return -1;
    return 0;
}

Packet *
TimeRange::simple_action(Packet *p)
{
    const Timestamp& tv = p->timestamp_anno();
    if (!_first)
	_first = _last = tv;
    else if (_simple)
	_last = tv;
    else if (_last < tv)
	_last = tv;
    else if (tv < _first)
	_first = tv;
    return p;
}

String
TimeRange::read_handler(Element *e, void *thunk)
{
    TimeRange *tr = static_cast<TimeRange *>(e);
    StringAccum sa;
    switch ((intptr_t)thunk) {
    case h_range:
	sa << tr->_first << ' ' << tr->_last;
	break;
    case h_interval:
	sa << (tr->_last - tr->_first);
	break;
    }
    return sa.take_string();
}

int
TimeRange::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    TimeRange *tr = static_cast<TimeRange *>(e);
    tr->_first = tr->_last = Timestamp();
    return 0;
}

void
TimeRange::add_handlers()
{
    add_data_handlers("first", Handler::f_read, &_first);
    add_data_handlers("last", Handler::f_read, &_last);
    add_read_handler("range", read_handler, h_range);
    add_read_handler("interval", read_handler, h_interval);
    add_write_handler("reset", write_handler, h_reset, Handler::f_button);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimeRange)
