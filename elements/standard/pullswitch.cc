/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "pullswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/llrpc.h>

PullSwitch::PullSwitch()
{
  MOD_INC_USE_COUNT;
  add_output();
}

PullSwitch::~PullSwitch()
{
  MOD_DEC_USE_COUNT;
}

PullSwitch *
PullSwitch::clone() const
{
  return new PullSwitch;
}

void
PullSwitch::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
PullSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _input = 0;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpInteger, "active input", &_input,
		  0) < 0)
    return -1;
  if (_input >= ninputs())
    _input = -1;
  return 0;
}

void
PullSwitch::configuration(Vector<String> &conf, bool *) const
{
  conf.push_back(String(_input));
}

Packet *
PullSwitch::pull(int)
{
  if (_input < 0)
    return 0;
  else
    return input(_input).pull();
}

String
PullSwitch::read_param(Element *e, void *)
{
  PullSwitch *sw = (PullSwitch *)e;
  return String(sw->_input) + "\n";
}

int
PullSwitch::write_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  PullSwitch *sw = (PullSwitch *)e;
  String s = cp_uncomment(in_s);
  if (!cp_integer(s, &sw->_input))
    return errh->error("PullSwitch input must be integer");
  if (sw->_input >= sw->ninputs())
    sw->_input = -1;
  return 0;
}

void
PullSwitch::add_handlers()
{
  add_read_handler("switch", read_param, (void *)0);
  add_write_handler("switch", write_param, (void *)0);
}

int
PullSwitch::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_SET_SWITCH) {
    int d;
    if (CLICK_LLRPC_GET_DATA(&d, data, sizeof(d)) < 0)
      return -EINVAL;
    _input = (d >= ninputs() ? -1 : d);
    return 0;

  } else if (command == CLICK_LLRPC_GET_SWITCH) {
    return CLICK_LLRPC_PUT_DATA(data, &_input, sizeof(_input));

  } else
    return Element::llrpc(command, data);
}

int
PullSwitch::local_llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_SET_SWITCH) {
    int *i = (int *)data;
    _input = (*i >= ninputs() ? -1 : *i);
    return 0;

  } else if (command == CLICK_LLRPC_GET_SWITCH) {
    int *i = (int *)data;
    *i = _input;
    return 0;

  } else
    return Element::local_llrpc(command, data);
}

EXPORT_ELEMENT(PullSwitch)
ELEMENT_MT_SAFE(PullSwitch)
