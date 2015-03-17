// -*- c-basic-offset: 4 -*-
/*
 * mixedqueue.{cc,hh} -- NotifierQueue with FIFO and LIFO inputs
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include "mixedqueue.hh"
CLICK_DECLS

MixedQueue::MixedQueue()
{
}

void *
MixedQueue::cast(const char *n)
{
    if (strcmp(n, "MixedQueue") == 0)
	return (MixedQueue *)this;
    else
	return NotifierQueue::cast(n);
}

void
MixedQueue::push(int port, Packet *p)
{
    Packet *oldp = 0;

    if (port == 0) {		// FIFO insert, drop new packet if full
	int h = head(), t = tail(), nt = next_i(t);
	if (nt == h) {
	    if (_drops == 0 && _capacity > 0)
		click_chatter("%p{element}: overflow", this);
	    _drops++;
	    checked_output_push(1, p);
	} else {
	    _q[t] = p;
	    set_tail(nt);
	}
    } else {			// LIFO insert, drop old packet if full
	int h = head(), t = tail(), ph = prev_i(h);
	if (ph == t) {
	    if (_drops == 0 && _capacity > 0)
		click_chatter("%p{element}: overflow", this);
	    _drops++;
	    t = prev_i(t);
	    oldp = _q[t];
	    set_tail_acquire(t);
	}
	_q[ph] = p;
	set_head_release(ph);
    }

    int s = size();
    if (s > _highwater_length)
	_highwater_length = s;
    if (s == 1 && !_empty_note.active())
	_empty_note.wake();

    if (oldp)
	checked_output_push(1, oldp);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(MixedQueue)
