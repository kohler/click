// -*- c-basic-offset: 4 -*-
/*
 * notifierqueue.{cc,hh} -- queue element that notifies when emptiness changes
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
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
#include "notifierqueue.hh"
CLICK_DECLS

NotifierQueue::NotifierQueue()
    : _sleepiness(0)
{
}

void *
NotifierQueue::cast(const char *n)
{
    if (strcmp(n, "NotifierQueue") == 0)
	return (NotifierQueue *)this;
    else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return static_cast<Notifier *>(&_empty_note);
    else
	return SimpleQueue::cast(n);
}

int
NotifierQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());
    return SimpleQueue::configure(conf, errh);
}

void
NotifierQueue::push(int, Packet *p)
{
    // Code taken from SimpleQueue::push().
    int h = _head, t = _tail, nt = next_i(t);

    if (nt != h) {
	_q[t] = p;
	packet_memory_barrier(_q[t], _tail);
	_tail = nt;

	int s = size(h, nt);
	if (s > _highwater_length)
	    _highwater_length = s;

	_empty_note.wake();

    } else {
	if (_drops == 0 && _capacity > 0)
	    click_chatter("%p{element}: overflow", this);
	_drops++;
	checked_output_push(1, p);
    }
}

Packet *
NotifierQueue::pull(int)
{
    Packet *p = deq();

    if (p)
	_sleepiness = 0;
    else if (_sleepiness >= SLEEPINESS_TRIGGER) {
	_empty_note.sleep();
#if HAVE_MULTITHREAD
	// Work around race condition between push() and pull().
	// We might have just undone push()'s Notifier::wake() call.
	// Easiest lock-free solution: check whether we should wake again!
	if (size())
	    _empty_note.wake();
#endif
    } else
	++_sleepiness;

    return p;
}

#if CLICK_DEBUG_SCHEDULING
String
NotifierQueue::read_handler(Element *e, void *)
{
    NotifierQueue *nq = static_cast<NotifierQueue *>(e);
    return "nonempty " + nq->_empty_note.unparse(nq->router());
}

void
NotifierQueue::add_handlers()
{
    add_read_handler("notifier_state", read_handler, 0);
    SimpleQueue::add_handlers();
}
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(SimpleQueue)
EXPORT_ELEMENT(NotifierQueue)
