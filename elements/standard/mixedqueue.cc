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
    set_ninputs(2);
    MOD_INC_USE_COUNT;
}

MixedQueue::~MixedQueue()
{
    MOD_DEC_USE_COUNT;
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
    if (port == 0) {		// FIFO insert, drop new packet if full
	int pindex = next_i(_tail);
	if (pindex == _head) {
	    if (_drops == 0)
		click_chatter("%{element}: overflow", this);
	    _drops++;
	    p->kill();
	} else {
	    _q[_tail] = p;
	    _tail = pindex;
	}
    } else {			// LIFO insert, drop old packet if full
	int pindex = prev_i(_head);
	if (pindex == _tail) {
	    if (_drops == 0)
		click_chatter("%{element}: overflow", this);
	    _drops++;
	    _tail = prev_i(_tail);
	    _q[_tail]->kill();
	}
	_q[pindex] = p;
	_head = pindex;
    }

    int s = size();
    if (s > _highwater_length)
	_highwater_length = s;
    if (s == 1 && listeners_asleep())
	wake_listeners();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(MixedQueue)
