/*
 * queue.{cc,hh} -- queue element
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
#include <click/config.h>
#include <click/package.hh>
#include "queue.hh"
#include <click/confparse.hh>
#include <click/error.hh>

Queue::Queue()
  : Element(1, 1), _q(0)
{
  MOD_INC_USE_COUNT;
}

Queue::~Queue()
{
  MOD_DEC_USE_COUNT;
  if (_q) uninitialize();
}

void *
Queue::cast(const char *n)
{
  if (strcmp(n, "Storage") == 0)
    return (Storage *)this;
  else if (strcmp(n, "Queue") == 0)
    return (Element *)this;
  else
    return 0;
}

int
Queue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int new_capacity = 1000;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "maximum queue length", &new_capacity,
		  0) < 0)
    return -1;
  _capacity = new_capacity;
  return 0;
}

int
Queue::initialize(ErrorHandler *errh)
{
  assert(!_q);
  _q = new Packet *[_capacity + 1];
  if (_q == 0)
    return errh->error("out of memory");

  _empty_jiffies = click_jiffies();
  _head = _tail = 0;
  _drops = 0;
  _highwater_length = 0;
  return 0;
}

int
Queue::live_reconfigure(const Vector<String> &conf, ErrorHandler *errh)
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
  for (i = _head, j = 0; i != _tail; i = next_i(i)) {
    new_q[j++] = _q[i];
    if (j == new_capacity) break;
  }
  for (; i != _tail; i = next_i(i))
    _q[i]->kill();
  
  delete[] _q;
  _q = new_q;
  _head = 0;
  _tail = j;
  _capacity = new_capacity;
  return 0;
}

void
Queue::take_state(Element *e, ErrorHandler *errh)
{
  Queue *q = (Queue *)e->cast("Queue");
  if (!q) return;
  
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
Queue::uninitialize()
{
  for (int i = _head; i != _tail; i = next_i(i))
    _q[i]->kill();
  delete[] _q;
  _q = 0;
}

void
Queue::push(int, Packet *packet)
{
  assert(packet);

  // inline Queue::enq() for speed
  int next = next_i(_tail);
  
  // should this stuff be in Queue::enq?
  if (next != _head) {
    _q[_tail] = packet;
    _tail = next;

#if 0
    /* Now taken care of by scheduler */
    
    /* comment is now a lie --  the problem is the same, the
       solution is different; FromDevice reschedules itself
       on busy, which means it will be run "soon", possibly
       sonner than if we relied on queue length passing a 16
       packet length boundary
    // The Linux net_bh() code processes all queued incoming
    // packets before checking whether output devices have
    // gone idle. Under high load this could leave outputs idle
    // even though packets are Queued. So cause output idleness
    // every 16 packets as well as when we go non-empty. */
    if (was_empty) {
      if (_puller1)
        _puller1->join_scheduler();
      else {
	int n = _pullers.size();
	for (int i = 0; i < n; i++)
          _pullers[i]->join_scheduler();
      }
    }
#endif
    
    int s = size();
    if (s > _highwater_length)
      _highwater_length = s;
    
  } else {
    // if (!(_drops % 100))
    if (_drops == 0)
      click_chatter("Queue %s overflow", id().cc());
    _drops++;
    packet->kill();
  }
}


Packet *
Queue::pull(int)
{
  return deq();
}


String
Queue::read_handler(Element *e, void *thunk)
{
  Queue *q = static_cast<Queue *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(q->size()) + "\n";
   case 1:
    return String(q->highwater_length()) + "\n";
   case 2:
    return String(q->capacity()) + "\n";
   case 3:
    return String(q->_drops) + "\n";
   default:
    return "";
  }
}

int
Queue::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
  Queue *q = static_cast<Queue *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    q->_drops = 0;
    q->_highwater_length = 0;
    return 0;
   case 1:
    while (q->_head != q->_tail) {
      int i = q->_head;
      q->_head = q->next_i(q->_head);
      q->_q[i]->kill();
    }
    return 0;
   default:
    return errh->error("internal error");
  }
}

void
Queue::add_handlers()
{
  add_read_handler("length", read_handler, (void *)0);
  add_read_handler("highwater_length", read_handler, (void *)1);
  add_read_handler("capacity", read_handler, (void *)2);
  add_read_handler("drops", read_handler, (void *)3);
  add_write_handler("capacity", reconfigure_write_handler, (void *)0);
  add_write_handler("reset_counts", write_handler, (void *)0);
  add_write_handler("reset", write_handler, (void *)1);
}

ELEMENT_PROVIDES(Storage)
EXPORT_ELEMENT(Queue)
