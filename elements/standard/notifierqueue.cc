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
    if (strcmp(n, "NotifierQueue") == 0)
	return (NotifierQueue *)this;
    else if (strcmp(n, "Notifier") == 0)
	return (Notifier *)this;
    else
	return Queue::cast(n);
}

int
NotifierQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Notifier::initialize(router());
    return Queue::configure(conf, errh);
}

void
NotifierQueue::push(int, Packet *packet)
{
    // wish I could inline...
    Queue::push(0, packet);

    if (size() == 1 && listeners_asleep())
	wake_listeners();
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

// XXX reset?

ELEMENT_REQUIRES(Queue)
EXPORT_ELEMENT(NotifierQueue)
