// -*- c-basic-offset: 4 -*-
/*
 * fullnotequeue.{cc,hh} -- queue element that notifies on full
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "fullnotequeue.hh"
CLICK_DECLS

FullNoteQueue::FullNoteQueue()
{
}

FullNoteQueue::~FullNoteQueue()
{
}

void *
FullNoteQueue::cast(const char *n)
{
    if (strcmp(n, "FullNoteQueue") == 0)
	return (FullNoteQueue *)this;
    else if (strcmp(n, Notifier::NONFULL_NOTIFIER) == 0)
	return static_cast<Notifier*>(&_full_note);
    else
	return NotifierQueue::cast(n);
}

int
FullNoteQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _full_note.initialize(router());
    _full_note.set_signal_active(true);
    return NotifierQueue::configure(conf, errh);
}

void
FullNoteQueue::push(int, Packet *p)
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
        if (!_empty_note.signal_active()) 
	    _empty_note.wake_listeners(); 
#else
        if (s == 1) {
            _lock.acquire();
	    if (!_empty_note.signal_active())
	        _empty_note.wake_listeners();
	    _lock.release();
	}
#endif

	if (s == capacity())
	    _full_note.sleep_listeners();

    } else {
	if (_drops == 0)
	    click_chatter("%{element}: overflow", this);
	_drops++;
	p->kill();
    }
}

Packet *
FullNoteQueue::pull(int)
{
    Packet *p = deq();

    if (p) {
	_sleepiness = 0;
	if (size() == capacity() - 1)
	    _full_note.wake_listeners();
	
    } else if (++_sleepiness == SLEEPINESS_TRIGGER) {
#if !NOTIFIERQUEUE_LOCK
        _empty_note.sleep_listeners();
#else
	_lock.acquire();
	if (_head == _tail)  // if still empty...
	    _empty_note.sleep_listeners();
	_lock.release();
#endif
    }

    return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(FullNoteQueue FullNoteQueue-FullNoteQueue)
