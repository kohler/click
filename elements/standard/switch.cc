/*
 * switch.{cc,hh} -- element routes packets to one output of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "switch.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/llrpc.h>

Switch::Switch()
{
  MOD_INC_USE_COUNT;
  add_input();
}

Switch::~Switch()
{
  MOD_DEC_USE_COUNT;
}

Switch *
Switch::clone() const
{
  return new Switch;
}

void
Switch::notify_noutputs(int n)
{
  set_noutputs(n);
}

int
Switch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _output = 0;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpInteger, "active output", &_output,
		  0) < 0)
    return -1;
  if (_output >= noutputs())
    _output = -1;
  return 0;
}

void
Switch::configuration(Vector<String> &conf) const
{
  conf.push_back(String(_output));
}

void
Switch::push(int, Packet *p)
{
  if (_output < 0)
    p->kill();
  else
    output(_output).push(p);
}

String
Switch::read_param(Element *e, void *)
{
  Switch *sw = (Switch *)e;
  return String(sw->_output) + "\n";
}

int
Switch::write_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  Switch *sw = (Switch *)e;
  String s = cp_uncomment(in_s);
  if (!cp_integer(s, &sw->_output))
    return errh->error("Switch output must be integer");
  if (sw->_output >= sw->noutputs())
    sw->_output = -1;
  return 0;
}

void
Switch::add_handlers()
{
  add_read_handler("switch", read_param, (void *)0);
  add_write_handler("switch", write_param, (void *)0);
}

int
Switch::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_SET_SWITCH) {
    int d;
    if (CLICK_LLRPC_GET_DATA(&d, data, sizeof(d)) < 0 || d != 0)
      return -EINVAL;
    _output = (d >= noutputs() ? -1 : d);
    return 0;

  } else if (command == CLICK_LLRPC_GET_SWITCH) {
    return CLICK_LLRPC_PUT_DATA(data, &_output, sizeof(_output));

  } else
    return Element::llrpc(command, data);
}

int
Switch::local_llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_SET_SWITCH) {
    int *i = (int *)data;
    _output = (*i >= noutputs() ? -1 : *i);
    return 0;

  } else if (command == CLICK_LLRPC_GET_SWITCH) {
    int *i = (int *)data;
    *i = _output;
    return 0;

  } else
    return Element::local_llrpc(command, data);
}

EXPORT_ELEMENT(Switch)
ELEMENT_MT_SAFE(Switch)
