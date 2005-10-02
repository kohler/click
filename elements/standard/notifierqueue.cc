// -*- c-basic-offset: 4 -*-
/*
 * notifierqueue.{cc,hh} -- queue element that notifies when emptiness changes
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
#include "notifierqueue.hh"
CLICK_DECLS

NotifierQueue::NotifierQueue()
    : _sleepiness(0)
{
}

NotifierQueue::~NotifierQueue()
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
    _empty_note.initialize(router());
    return SimpleQueue::configure(conf, errh);
}

void
NotifierQueue::push(int, Packet *p)
{
    // Code taken from SimpleQueue::push().
    int next = next_i(_tail);

    if (next != _head) {
	_q[_tail] = p;
	_tail = next;

	int s = size();
	if (s > _highwater_length)
	    _highwater_length = s;

#if !NOTIFIERQUEUE_LOCK
	// This can leave a single packet in the queue indefinitely in
	// multithreaded Click, because of a race condition with pull().
        if (!_empty_note.active()) 
	    _empty_note.wake(); 
#else
        if (s == 1) {
            _lock.acquire();
	    if (!_empty_note.active())
	        _empty_note.wake();
	    _lock.release();
	}
#endif

    } else {
	if (_drops == 0)
	    click_chatter("%{element}: overflow", this);
	_drops++;
	p->kill();
    }
}

Packet *
NotifierQueue::pull(int)
{
    Packet *p = deq();

    if (p)
	_sleepiness = 0;
    else if (++_sleepiness == SLEEPINESS_TRIGGER) {
#if !NOTIFIERQUEUE_LOCK
        _empty_note.sleep();
#else
	_lock.acquire();
	if (_head == _tail)  // if still empty...
	    _empty_note.sleep();
	_lock.release();
#endif
    }

    return p;
}

#if NOTIFIERQUEUE_DEBUG
#include <click/straccum.hh>

String
NotifierQueue::read_handler(Element *e, void *)
{
    StringAccum sa;
    NotifierQueue *nq = static_cast<NotifierQueue *>(e);
    sa << "notifier " << (nq->_empty_note.active() ? "on" : "off") << '\n';
    Vector<Task *> v;
    nq->_empty_note.listeners(v);
    for (int i = 0; i < v.size(); i++) {
	sa << "task " << ((void *)v[i]) << ' ';
	if (Element *e = v[i]->element())
	    sa << '[' << e->declaration() << "] ";
	sa << (v[i]->scheduled() ? "scheduled\n" : "unscheduled\n");
    }
    return sa.take_string();
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
