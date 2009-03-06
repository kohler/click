// -*- c-basic-offset: 4 -*-
/*
 * simplequeue.{cc,hh} -- queue element
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "simplequeue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

SimpleQueue::SimpleQueue()
    : _q(0)
{
}

SimpleQueue::~SimpleQueue()
{
}

void *
SimpleQueue::cast(const char *n)
{
    if (strcmp(n, "Storage") == 0)
	return (Storage *)this;
    else if (strcmp(n, "SimpleQueue") == 0
	     || strcmp(n, "Queue") == 0)
	return (Element *)this;
    else
	return 0;
}

int
SimpleQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int new_capacity = 1000;
    if (cp_va_kparse(conf, this, errh,
		     "CAPACITY", cpkP, cpUnsigned, &new_capacity,
		     cpEnd) < 0)
	return -1;
    _capacity = new_capacity;
    return 0;
}

int
SimpleQueue::initialize(ErrorHandler *errh)
{
    assert(!_q && _head == 0 && _tail == 0);
    _q = (Packet **) CLICK_LALLOC(sizeof(Packet *) * (_capacity + 1));
    if (_q == 0)
	return errh->error("out of memory");
    _drops = 0;
    _highwater_length = 0;
    return 0;
}

int
SimpleQueue::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    // change the maximum queue length at runtime
    int old_capacity = _capacity;
    // NB: do not call children!
    if (SimpleQueue::configure(conf, errh) < 0)
	return -1;
    if (_capacity == old_capacity || !_q)
	return 0;
    int new_capacity = _capacity;
    _capacity = old_capacity;

    Packet **new_q = (Packet **) CLICK_LALLOC(sizeof(Packet *) * (new_capacity + 1));
    if (new_q == 0)
	return errh->error("out of memory");

    int i, j;
    for (i = _head, j = 0; i != _tail && j != new_capacity; i = next_i(i))
	new_q[j++] = _q[i];
    for (; i != _tail; i = next_i(i))
	_q[i]->kill();

    CLICK_LFREE(_q, sizeof(Packet *) * (_capacity + 1));
    _q = new_q;
    _head = 0;
    _tail = j;
    _capacity = new_capacity;
    return 0;
}

void
SimpleQueue::take_state(Element *e, ErrorHandler *errh)
{
    SimpleQueue *q = (SimpleQueue *)e->cast("SimpleQueue");
    if (!q)
	return;

    if (_tail != _head || _head != 0) {
	errh->error("already have packets enqueued, can't take state");
	return;
    }

    _head = 0;
    int i = 0, j = q->_head;
    while (i < _capacity && j != q->_tail) {
	_q[i] = q->_q[j];
	i++;
	j = q->next_i(j);
    }
    _tail = i;
    _highwater_length = size();

    if (j != q->_tail)
	errh->warning("some packets lost (old length %d, new capacity %d)",
		      q->size(), _capacity);
    while (j != q->_tail) {
	q->_q[j]->kill();
	j = q->next_i(j);
    }
    q->set_head(0);
    q->set_tail(0);
}

void
SimpleQueue::cleanup(CleanupStage)
{
    for (int i = _head; i != _tail; i = next_i(i))
	_q[i]->kill();
    CLICK_LFREE(_q, sizeof(Packet *) * (_capacity + 1));
    _q = 0;
}

void
SimpleQueue::push(int, Packet *p)
{
    // If you change this code, also change NotifierQueue::push()
    // and FullNoteQueue::push().
    int h = _head, t = _tail, nt = next_i(t);

    // should this stuff be in SimpleQueue::enq?
    if (nt != h) {
	_q[t] = p;
	// memory barrier here
	_tail = nt;

	int s = size(h, nt);
	if (s > _highwater_length)
	    _highwater_length = s;

    } else {
	// if (!(_drops % 100))
	if (_drops == 0)
	    click_chatter("%{element}: overflow", this);
	_drops++;
	checked_output_push(1, p);
    }
}

Packet *
SimpleQueue::pull(int)
{
    return deq();
}

#if 0
Vector<Packet *>
SimpleQueue::yank(bool (filter)(const Packet *, void *), void *thunk)
{
    // remove all packets from the queue that match filter(); return in
    // a vector.  caller is responsible for managing the yank()-ed
    // packets from now on, i.e. deallocating them.
    Vector<Packet *> v;

    int next_slot = _head;
    for (int i = _head; i != _tail; i = next_i(i)) {
	if (filter(_q[i], thunk))
	    v.push_back(_q[i]);
	else {
	    _q[next_slot] = _q[i];
	    next_slot = next_i(next_slot);
	}
    }
    _tail = next_slot;

    return v;
}
#endif


String
SimpleQueue::read_handler(Element *e, void *thunk)
{
    SimpleQueue *q = static_cast<SimpleQueue *>(e);
    int which = reinterpret_cast<intptr_t>(thunk);
    switch (which) {
      case 0:
	return String(q->size());
      case 1:
	return String(q->highwater_length());
      case 2:
	return String(q->capacity());
      case 3:
	return String(q->_drops);
      default:
	return "";
    }
}

void
SimpleQueue::reset()
{
    while (Packet *p = pull(0))
	checked_output_push(1, p);
}

int
SimpleQueue::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
    SimpleQueue *q = static_cast<SimpleQueue *>(e);
    int which = reinterpret_cast<intptr_t>(thunk);
    switch (which) {
      case 0:
	q->_drops = 0;
	q->_highwater_length = q->size();
	return 0;
      case 1:
	q->reset();
	return 0;
      default:
	return errh->error("internal error");
    }
}

void
SimpleQueue::add_handlers()
{
    add_read_handler("length", read_handler, (void *)0);
    add_read_handler("highwater_length", read_handler, (void *)1);
    add_read_handler("capacity", read_handler, (void *)2, Handler::CALM);
    add_read_handler("drops", read_handler, (void *)3);
    add_write_handler("capacity", reconfigure_keyword_handler, "0 CAPACITY");
    add_write_handler("reset_counts", write_handler, (void *)0, Handler::BUTTON | Handler::NONEXCLUSIVE);
    add_write_handler("reset", write_handler, (void *)1, Handler::BUTTON);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Storage)
EXPORT_ELEMENT(SimpleQueue)
