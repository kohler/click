// -*- c-basic-offset: 4 -*-
/*
 * frontdropqueue.{cc,hh} -- queue element that drops front when full
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
#include "frontdropqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

FrontDropQueue::FrontDropQueue()
{
}

void *
FrontDropQueue::cast(const char *n)
{
  if (strcmp(n, "FrontDropQueue") == 0)
    return (FrontDropQueue *)this;
  else
    return NotifierQueue::cast(n);
}

int
FrontDropQueue::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
  // change the maximum queue length at runtime
  Storage::index_type old_capacity = _capacity;
  if (configure(conf, errh) < 0)
    return -1;
  if (_capacity == old_capacity || !_q)
    return 0;
  Storage::index_type new_capacity = _capacity;
  _capacity = old_capacity;

  Packet **new_q = (Packet **) CLICK_LALLOC(sizeof(Packet *) * (new_capacity + 1));
  if (new_q == 0)
    return errh->error("out of memory");

  Storage::index_type i = tail(), j = new_capacity;
  while (j != 0 && i != head()) {
      i = prev_i(i);
      --j;
      new_q[j] = _q[i];
  }
  while (i != head()) {
      i = prev_i(i);
      _q[i]->kill();
  }

  CLICK_LFREE(_q, sizeof(Packet *) * (_capacity + 1));
  _q = new_q;
  set_head(j);
  set_tail(new_capacity);
  _capacity = new_capacity;
  return 0;
}

void
FrontDropQueue::take_state(Element *e, ErrorHandler *errh)
{
    SimpleQueue *q = (SimpleQueue *)e->cast("SimpleQueue");
    if (!q)
	return;

    if (tail() != head() || head() != 0) {
	errh->error("already have packets enqueued, can%,t take state");
	return;
    }

    set_tail(_capacity);
    Storage::index_type i = _capacity, j = q->tail();
    while (i > 0 && j != q->head()) {
	i--;
	j = q->prev_i(j);
	_q[i] = q->packet(j);
    }
    set_head(i);
    _highwater_length = size();

    if (j != q->head())
	errh->warning("some packets lost (old length %d, new capacity %d)",
		      q->size(), _capacity);
    while (j != q->head()) {
	j = q->prev_i(j);
	q->packet(j)->kill();
    }
    q->set_head(0);
    q->set_tail(0);
}

void
FrontDropQueue::push(int, Packet *p)
{
    assert(p);

    // inline Queue::enq() for speed
    Storage::index_type t = tail(), nt = next_i(t);

    // should this stuff be in Queue::enq?
    if (nt == head()) {
	if (_drops == 0 && _capacity > 0)
	    click_chatter("%p{element}: overflow", this);
	checked_output_push(1, _q[nt]);
	_drops++;
	set_head(next_i(nt));
    }

    _q[t] = p;
    set_tail(nt);

    int s = size();
    if (s > _highwater_length)
	_highwater_length = s;
    if (s == 1 && !_empty_note.active())
	_empty_note.wake();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(FrontDropQueue)
