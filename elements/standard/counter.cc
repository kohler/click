/*
 * counter.{cc,hh} -- element counts packets, measures packet rate
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
#include "counter.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/handlercall.hh>
CLICK_DECLS

#ifdef HAVE_INT64_TYPES
# define PARSECMD cp_unsigned64
#else
# define PARSECMD cp_unsigned
#endif

Counter::Counter()
  : Element(1, 1), _count_trigger_h(0), _byte_trigger_h(0)
{
  MOD_INC_USE_COUNT;
}

Counter::~Counter()
{
  MOD_DEC_USE_COUNT;
  delete _count_trigger_h;
  delete _byte_trigger_h;
}

void
Counter::reset()
{
  _count = _byte_count = 0;
  _rate.initialize();
  _count_triggered = _byte_triggered = false;
}

int
Counter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String count_call, byte_count_call;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords,
		  "COUNT_CALL", cpArgument, "handler to call after a count", &count_call,
		  "BYTE_COUNT_CALL", cpArgument, "handler to call after a byte count", &byte_count_call,
		  0) < 0)
    return -1;

  if (count_call) {
    if (!PARSECMD(cp_pop_spacevec(count_call), &_count_trigger))
      return errh->error("`COUNT_CALL' first word should be unsigned (count)");
    _count_trigger_h = new HandlerCall(count_call);
  } else
    _count_trigger = (counter_t)(-1);

  if (byte_count_call) {
    if (!PARSECMD(cp_pop_spacevec(byte_count_call), &_byte_trigger))
      return errh->error("`BYTE_COUNT_CALL' first word should be unsigned (count)");
    _byte_trigger_h = new HandlerCall(byte_count_call);
  } else
    _byte_trigger = (counter_t)(-1);

  return 0;
}

int
Counter::initialize(ErrorHandler *errh)
{
  if (_count_trigger_h && _count_trigger_h->initialize_write(this, errh) < 0)
    return -1;
  if (_byte_trigger_h && _byte_trigger_h->initialize_write(this, errh) < 0)
    return -1;
  reset();
  return 0;
}

Packet *
Counter::simple_action(Packet *p)
{
  _count++;
  _byte_count += p->length();
  _rate.update(1);

  if (_count == _count_trigger && !_count_triggered) {
    _count_triggered = true;
    if (_count_trigger_h)
      (void) _count_trigger_h->call_write(this);
  }
  if (_byte_count >= _byte_trigger && !_byte_triggered) {
    _byte_triggered = true;
    if (_byte_trigger_h)
      (void) _byte_trigger_h->call_write(this);
  }
  
  return p;
}


enum { H_COUNT, H_BYTE_COUNT, H_RATE, H_RESET,
       H_COUNT_CALL, H_BYTE_COUNT_CALL };

String
Counter::read_handler(Element *e, void *thunk)
{
  Counter *c = (Counter *)e;
  switch ((intptr_t)thunk) {
   case H_COUNT:
    return String(c->_count) + "\n";
   case H_BYTE_COUNT:
    return String(c->_byte_count) + "\n";
   case H_RATE:
    c->_rate.update_time();	// drop rate after zero period
    return c->_rate.unparse() + "\n";
   default:
    return "<error>\n";
  }
}

int
Counter::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
  Counter *c = (Counter *)e;
  String str = cp_uncomment(in_str);
  switch ((intptr_t)thunk) {
   case H_COUNT_CALL:
    if (!PARSECMD(cp_pop_spacevec(str), &c->_count_trigger))
      return errh->error("`count_call' first word should be unsigned (count)");
    if (HandlerCall::initialize(c->_count_trigger_h, str, true, c, errh) < 0)
      return -1;
    c->_count_triggered = false;
    return 0;
   case H_BYTE_COUNT_CALL:
    if (!PARSECMD(cp_pop_spacevec(str), &c->_byte_trigger))
      return errh->error("`byte_count_call' first word should be unsigned (count)");
    if (HandlerCall::initialize(c->_byte_trigger_h, str, true, c, errh) < 0)
      return -1;
    c->_byte_triggered = false;
    return 0;
   case H_RESET:
    c->reset();
    return 0;
   default:
    return errh->error("<internal>");
  }
}

void
Counter::add_handlers()
{
  add_read_handler("count", read_handler, (void *)H_COUNT);
  add_read_handler("byte_count", read_handler, (void *)H_BYTE_COUNT);
  add_read_handler("rate", read_handler, (void *)H_RATE);
  add_write_handler("reset", write_handler, (void *)H_RESET);
  add_write_handler("count_call", write_handler, (void *)H_COUNT_CALL);
  add_write_handler("byte_count_call", write_handler, (void *)H_BYTE_COUNT_CALL);
}

int
Counter::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_GET_RATE) {
    uint32_t *val = reinterpret_cast<uint32_t *>(data);
    if (*val != 0)
      return -EINVAL;
    _rate.update_time();	// drop rate after zero period
    *val = _rate.rate();
    return 0;

  } else if (command == CLICK_LLRPC_GET_COUNT) {
    uint32_t *val = reinterpret_cast<uint32_t *>(data);
    if (*val != 0 && *val != 1)
      return -EINVAL;
    *val = (*val == 0 ? _count : _byte_count);
    return 0;
    
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

CLICK_ENDDECLS
EXPORT_ELEMENT(Counter)
