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
  : Element(1, 1), _q(0)
{
  MOD_INC_USE_COUNT;
}

SimpleQueue::~SimpleQueue()
{
  MOD_DEC_USE_COUNT;
}

void *
SimpleQueue::cast(const char *n)
{
  if (strcmp(n, "Storage") == 0)
    return (Storage *)this;
  else if (strcmp(n, "SimpleQueue") == 0)
    return (Element *)this;
  else
    return 0;
}

int
SimpleQueue::configure(Vector<String> &conf, ErrorHandler *errh)
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
SimpleQueue::initialize(ErrorHandler *errh)
{
  assert(!_q && _head == 0 && _tail == 0);
  _q = new Packet *[_capacity + 1];
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
SimpleQueue::take_state(Element *e, ErrorHandler *errh)
{
  SimpleQueue *q = (SimpleQueue *)e->cast("SimpleQueue");
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
SimpleQueue::cleanup(CleanupStage)
{
  for (int i = _head; i != _tail; i = next_i(i))
    _q[i]->kill();
  delete[] _q;
  _q = 0;
}

void
SimpleQueue::push(int, Packet *p)
{
  // If you change this code, also change NotifierQueue::push().
  int next = next_i(_tail);
  
  // should this stuff be in SimpleQueue::enq?
  if (next != _head) {
    _q[_tail] = p;
    _tail = next;

    int s = size();
    if (s > _highwater_length)
      _highwater_length = s;
    
  } else {
    // if (!(_drops % 100))
    if (_drops == 0)
      click_chatter("%{element}: overflow", this);
    _drops++;
    p->kill();
  }
}

Packet *
SimpleQueue::pull(int)
{
  return deq();
}

Vector<Packet *>
SimpleQueue::yank(bool (filter)(const Packet *))
{
  // remove all packets from the queue that match filter(); return in
  // a vector.  caller is responsible for managing the yank()-ed
  // packets from now on, i.e. deallocating them.
  Vector<Packet *> v;
  
  int next_slot = _head;
  for (int i = _head; i != _tail; i = next_i(i)) {
    if (filter(_q[i]))
      v.push_back(_q[i]);
    else {
      _q[next_slot] = _q[i];
      next_slot = next_i(next_slot);
    }
  }
  _tail = next_slot;

  return v;
}


String
SimpleQueue::read_handler(Element *e, void *thunk)
{
  SimpleQueue *q = static_cast<SimpleQueue *>(e);
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
SimpleQueue::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
  SimpleQueue *q = static_cast<SimpleQueue *>(e);
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
SimpleQueue::add_handlers()
{
  add_read_handler("length", read_handler, (void *)0);
  add_read_handler("highwater_length", read_handler, (void *)1);
  add_read_handler("capacity", read_handler, (void *)2);
  add_read_handler("drops", read_handler, (void *)3);
  add_write_handler("capacity", reconfigure_positional_handler, (void *)0);
  add_write_handler("reset_counts", write_handler, (void *)0);
  add_write_handler("reset", write_handler, (void *)1);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Storage)
EXPORT_ELEMENT(SimpleQueue)
