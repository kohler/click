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
    MOD_INC_USE_COUNT;
}

NotifierQueue::~NotifierQueue()
{
    MOD_DEC_USE_COUNT;
}

void *
NotifierQueue::cast(const char *n)
{
    if (strcmp(n, "Queue") == 0)
	return (NotifierQueue *)this;
    else if (strcmp(n, "Notifier") == 0)
	return (Notifier *)this;
    else
	return SimpleQueue::cast(n);
}

int
NotifierQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Notifier::initialize(router());
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
	if (s == 1 && listeners_asleep())
	    wake_listeners();

    } else {
	// if (!(_drops % 100))
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
    else if (++_sleepiness == SLEEPINESS_TRIGGER)
	sleep_listeners();
    
    return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(SimpleQueue)
EXPORT_ELEMENT(NotifierQueue NotifierQueue-NotifierQueue)
