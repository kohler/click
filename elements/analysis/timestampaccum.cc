// -*- c-basic-offset: 4 -*-
/*
 * timestampaccum.{cc,hh} -- accumulate cycle counter deltas
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "timestampaccum.hh"
#include <click/glue.hh>
CLICK_DECLS

TimestampAccum::TimestampAccum()
{
}

TimestampAccum::~TimestampAccum()
{
}

int
TimestampAccum::initialize(ErrorHandler *)
{
    _usec_accum = 0;
    _count = 0;
    return 0;
}

inline Packet *
TimestampAccum::simple_action(Packet *p)
{
    _usec_accum += (Timestamp::now() - p->timestamp_anno()).doubleval();
    _count++;
    return p;
}

String
TimestampAccum::read_handler(Element *e, void *thunk)
{
    TimestampAccum *ta = static_cast<TimestampAccum *>(e);
    int which = reinterpret_cast<intptr_t>(thunk);
    switch (which) {
      case 0:
	return String(ta->_count);
      case 1:
	return String(ta->_usec_accum);
      case 2:
	return String(ta->_usec_accum / ta->_count);
      default:
	return String();
    }
}

int
TimestampAccum::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
    TimestampAccum *ta = static_cast<TimestampAccum *>(e);
    ta->_usec_accum = 0;
    ta->_count = 0;
    return 0;
}

void
TimestampAccum::add_handlers()
{
    add_read_handler("count", read_handler, 0);
    add_read_handler("time", read_handler, 1);
    add_read_handler("average_time", read_handler, 2);
    add_write_handler("reset_counts", reset_handler, 0, Handler::f_button);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel int64)
EXPORT_ELEMENT(TimestampAccum)
