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
    if (r >= 0 && size() < capacity() && _q)
	_full_note.wake();
    _xhead = _head;
    _xtail = _tail;
    return r;
}

void
ThreadSafeQueue::push(int, Packet *p)
{
    // Code taken from SimpleQueue::push().

    // Reserve a slot by incrementing _xtail
    Storage::index_type t, nt;
    do {
	t = _tail;
	nt = next_i(t);
    } while (_xtail.compare_swap(t, nt) != t);
    // Other pushers spin until _tail := nt (or _xtail := t)

    Storage::index_type h = _head;
    if (nt != h)
	push_success(h, t, nt, p);
    else {
	_xtail = t;
	push_failure(p);
    }
}

Packet *
ThreadSafeQueue::pull(int)
{
    // Code taken from SimpleQueue::deq.

    // Reserve a slot by incrementing _xhead
    Storage::index_type h, nh;
    do {
	h = _head;
	nh = next_i(h);
    } while (_xhead.compare_swap(h, nh) != h);
    // Other pullers spin until _head := nh (or _xhead := h)

    Storage::index_type t = _tail;
    if (t != h)
	return pull_success(h, nh);
    else {
	_xhead = h;
	return pull_failure();
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FullNoteQueue)
EXPORT_ELEMENT(ThreadSafeQueue)
