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

#ifdef HAVE_INT64_TYPES
static CpVaParseCmd parse_cmd = cpUnsigned64;
#else
static CpVaParseCmd parse_cmd = cpUnsigned;
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
  String call_after_count, call_after_bytes;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords,
		  "CALL_AFTER_COUNT", cpArgument, "handler to call after a count", &call_after_count,
		  "CALL_AFTER_BYTES", cpArgument, "handler to call after a byte count", &call_after_bytes,
		  0) < 0)
    return -1;

  if (call_after_count) {
    if (cp_va_space_parse(call_after_count, this, errh,
			  parse_cmd, "count trigger", &_count_trigger,
			  cpWriteHandlerCall, "handler to call", &_count_trigger_h,
			  0) < 0)
      return -1;
  } else
    _count_trigger = (counter_t)(-1);

  if (call_after_bytes) {
    if (cp_va_space_parse(call_after_bytes, this, errh,
			  parse_cmd, "bytecount trigger", &_byte_trigger,
			  cpWriteHandlerCall, "handler to call", &_byte_trigger_h,
			  0) < 0)
      return -1;
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
       H_CALL_AFTER_COUNT, H_CALL_AFTER_BYTES };

String
Counter::read_handler(Element *e, void *thunk)
{
  Counter *c = (Counter *)e;
  switch ((int)thunk) {
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
  switch ((int)thunk) {
   case H_CALL_AFTER_COUNT:
    if (cp_va_space_parse(in_str, c, errh,
			  parse_cmd, "count trigger", &c->_count_trigger,
			  cpOptional,
			  cpWriteHandlerCall, "handler to call", &c->_count_trigger_h, 0) < 0)
      return -1;
    c->_count_triggered = false;
    return 0;
   case H_CALL_AFTER_BYTES:
    if (cp_va_space_parse(in_str, c, errh,
			  parse_cmd, "bytecount trigger", &c->_byte_trigger,
			  cpOptional,
			  cpWriteHandlerCall, "handler to call", &c->_byte_trigger_h, 0) < 0)
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
  add_write_handler("call_after_count", write_handler, (void *)H_CALL_AFTER_COUNT);
  add_write_handler("call_after_bytes", write_handler, (void *)H_CALL_AFTER_BYTES);
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


EXPORT_ELEMENT(Counter)
