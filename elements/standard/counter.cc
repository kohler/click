/*
 * counter.{cc,hh} -- element counts packets, measures packet rate
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

#include <click/config.h>
#include "counter.hh"
#include <click/confparse.hh>
#include <click/error.hh>

Counter::Counter()
  : Element(1, 1), _count(0)
{
  MOD_INC_USE_COUNT;
}

Counter::~Counter()
{
  MOD_DEC_USE_COUNT;
}

void
Counter::reset()
{
  _count = _byte_count = 0;
  _rate.initialize();
}

int
Counter::initialize(ErrorHandler *)
{
  reset();
  return 0;
}

Packet *
Counter::simple_action(Packet *p)
{
  _count++;
  _byte_count += p->length();
  _rate.update(1);
  return p;
}

String
Counter::read_handler(Element *e, void *thunk)
{
  Counter *c = (Counter *)e;
  switch ((int)thunk) {
   case 0:
    return String(c->_count) + "\n";
   case 1:
    return String(c->_byte_count) + "\n";
   case 2:
    return c->_rate.unparse() + "\n";
   default:
    return "<error>\n";
  }
}

static int
counter_reset_write_handler(const String &, Element *e, void *, ErrorHandler *)
{
  Counter *c = (Counter *)e;
  c->reset();
  return 0;
}

void
Counter::add_handlers()
{
  add_read_handler("count", read_handler, (void *)0);
  add_read_handler("byte_count", read_handler, (void *)1);
  add_read_handler("rate", read_handler, (void *)2);
  add_write_handler("reset", counter_reset_write_handler, 0);
}

int
Counter::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_GET_RATE) {
    unsigned d;
    if (CLICK_LLRPC_GET_DATA(&d, data, sizeof(d)) < 0 || d != 0)
      return -EINVAL;
    unsigned r = _rate.rate();
    return CLICK_LLRPC_PUT_DATA(data, &r, sizeof(r));

  } else if (command == CLICK_LLRPC_GET_COUNT) {
    unsigned d;
    if (CLICK_LLRPC_GET_DATA(&d, data, sizeof(d)) < 0 || (d != 0 && d != 1))
      return -EINVAL;
    unsigned *what = (d == 0 ? &_count : &_byte_count);
    return CLICK_LLRPC_PUT_DATA(data, what, sizeof(*what));
    
  } else if (command == CLICK_LLRPC_GET_COUNTS) {
    click_llrpc_counts_st *user_cs = (click_llrpc_counts_st *)data;
    click_llrpc_counts_st cs;
    if (CLICK_LLRPC_GET_DATA(&cs, data, sizeof(cs.n) + sizeof(cs.keys)) < 0
	|| cs.n >= CLICK_LLRPC_COUNTS_SIZE)
      return -EINVAL;
    for (unsigned i = 0; i < cs.n; i++) {
      if (cs.keys[i] == 0)
	cs.values[i] = _count;
      else if (cs.keys[i] == 1)
	cs.values[i] = _byte_count;
      else
	return -EINVAL;
    }
    return CLICK_LLRPC_PUT_DATA(&user_cs->values, &cs.values, sizeof(cs.values));
    
  } else
    return Element::llrpc(command, data);
}


EXPORT_ELEMENT(Counter)
