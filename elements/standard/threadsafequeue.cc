// -*- c-basic-offset: 4 -*-
/*
 * threadsafequeue.{cc,hh} -- queue element safe for use on SMP
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "threadsafequeue.hh"
CLICK_DECLS

ThreadSafeQueue::ThreadSafeQueue()
{
    _xhead = _xtail = 0;
}

ThreadSafeQueue::~ThreadSafeQueue()
{
}

void *
ThreadSafeQueue::cast(const char *n)
{
    if (strcmp(n, "ThreadSafeQueue") == 0)
	return (ThreadSafeQueue *)this;
    else
	return FullNoteQueue::cast(n);
}

int
ThreadSafeQueue::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    int r = NotifierQueue::live_reconfigure(conf, errh);
    if (r >= 0 && size() < capacity())
	_full_note.wake();
    _xhead = _head;
    _xtail = _tail;
    return r;
}

void
ThreadSafeQueue::push(int, Packet *p)
{
    // Code taken from SimpleQueue::push().
    int h, t, nt;

    do {
	h = _head;
	t = _tail;
	nt = next_i(t);

	if (nt == h) {
	    push_failure(p);
	    return;
	}
    } while (!_xtail.compare_and_swap(t, nt));

    push_success(h, t, nt, p);
}

Packet *
ThreadSafeQueue::pull(int)
{
    // Code taken from SimpleQueue::deq.
    int h, t, nh;

    do {
	h = _head;
	t = _tail;
	nh = next_i(h);

	if (h == t)
	    return pull_failure();
    } while (!_xhead.compare_and_swap(h, nh));

    return pull_success(h, t, nh);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FullNoteQueue multithread)
EXPORT_ELEMENT(ThreadSafeQueue)
