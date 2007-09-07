// -*- c-basic-offset: 4 -*-
/*
 * fullnotequeue.{cc,hh} -- queue element that notifies on full
 * Eddie Kohler
 *
 * Copyright (c) 2004-2007 Regents of the University of California
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
    else if (strcmp(n, Notifier::FULL_NOTIFIER) == 0)
	return static_cast<Notifier*>(&_full_note);
    else
	return NotifierQueue::cast(n);
}

int
FullNoteQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _full_note.initialize(router());
    _full_note.set_active(true, false);
    return NotifierQueue::configure(conf, errh);
}

void
FullNoteQueue::push(int, Packet *p)
{
    // Code taken from SimpleQueue::push().
    int h = _head, t = _tail, nt = next_i(t);

    if (nt != h) {
	_q[t] = p;
	// memory barrier here
	_tail = nt;

	int s = size(h, nt);
	if (s > _highwater_length)
	    _highwater_length = s;

	_empty_note.wake(); 

	if (s == capacity()) {
	    _full_note.sleep();
#if __MTCLICK__
	    // Work around race condition between push() and pull().
	    // We might have just undone pull()'s Notifier::wake() call.
	    // Easiest lock-free solution: check whether we should wake again!
	    if (size() < capacity())
		_full_note.wake();
#endif
	}

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
    // Code taken from SimpleQueue::deq.
    int h = _head, t = _tail, nh = next_i(h);

    if (h != t) {
	Packet *p = _q[h];
	// memory barrier here
	_head = nh;

	_sleepiness = 0;

	_full_note.wake();
	
	return p;
	
    } else if (++_sleepiness == SLEEPINESS_TRIGGER) {
        _empty_note.sleep();
#if __MTCLICK__
	// Work around race condition between push() and pull().
	// We might have just undone push()'s Notifier::wake() call.
	// Easiest lock-free solution: check whether we should wake again!
	if (size())
	    _empty_note.wake();
#endif
    }

    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(FullNoteQueue FullNoteQueue-FullNoteQueue)
