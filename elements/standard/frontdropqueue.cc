/*
 * frontdropqueue.{cc,hh} -- queue element that drops front when full
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "frontdropqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>

FrontDropQueue::FrontDropQueue()
{
}

FrontDropQueue::~FrontDropQueue()
{
}

void *
FrontDropQueue::cast(const char *n)
{
  if (strcmp(n, "FrontDropQueue") == 0)
    return (FrontDropQueue *)this;
  else
    return Queue::cast(n);
}

int
FrontDropQueue::live_reconfigure(const Vector<String> &conf, ErrorHandler *errh)
{
  // change the maximum queue length at runtime
  int old_capacity = _capacity;
  if (configure(conf, errh) < 0)
    return -1;
  if (_capacity == old_capacity)
    return 0;
  int new_capacity = _capacity;
  _capacity = old_capacity;
  
  Packet **new_q = new Packet *[new_capacity + 1];
  if (new_q == 0)
    return errh->error("out of memory");
  
  int i, j;
  for (i = _tail - 1, j = new_capacity; i != _head; i = prev_i(i)) {
    new_q[--j] = _q[i];
    if (j == 0) break;
  }
  for (; i != _head; i = prev_i(i))
    _q[i]->kill();
  
  delete[] _q;
  _q = new_q;
  _head = j;
  _tail = new_capacity;
  _capacity = new_capacity;
  return 0;
}

void
FrontDropQueue::take_state(Element *e, ErrorHandler *errh)
{
  Queue *q = (Queue *)e->cast("Queue");
  if (!q) return;
  
  if (_tail != _head || _head != 0) {
    errh->error("already have packets enqueued, can't take state");
    return;
  }
  
  _tail = _capacity;
  int i = _capacity, j = q->_tail;
  while (i > 0 && j != q->_head) {
    i--;
    j = q->prev_i(j);
    _q[i] = q->_q[j];
  }
  _head = i;
  _highwater_length = size();

  if (j != q->_head)
    errh->warning("some packets lost (old length %d, new capacity %d)",
		  q->size(), _capacity);
  while (j != q->_head) {
    j = q->prev_i(j);
    q->_q[j]->kill();
  }
  q->_head = q->_tail = 0;
}

void
FrontDropQueue::push(int, Packet *packet)
{
  assert(packet);

  // inline Queue::enq() for speed
  int next = next_i(_tail);
  
  // should this stuff be in Queue::enq?
  if (next == _head) {
    _q[_head]->kill();
    _drops++;
    _head++;
  }
  
  _q[_tail] = packet;
  _tail = next;
  
  int s = size();
  if (s > _highwater_length)
    _highwater_length = s;
}

ELEMENT_REQUIRES(Queue)
EXPORT_ELEMENT(FrontDropQueue)
