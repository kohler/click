/*
 * switch.{cc,hh} -- element routes packets to one output of several
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
#include "switch.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/llrpc.h>
CLICK_DECLS

Switch::Switch()
{
}

int
Switch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int output = 0;
    if (Args(conf, this, errh).read_p("OUTPUT", output).complete() < 0)
	return -1;
    if (output >= noutputs())
	return errh->error("output must be < %d", noutputs());
    _output = output;
    return 0;
}

void
Switch::push(int, Packet *p)
{
  checked_output_push(_output, p);
}

String
Switch::read_param(Element *e, void *)
{
  Switch *sw = (Switch *)e;
  return String(sw->_output);
}

int
Switch::write_param(const String &s, Element *e, void *, ErrorHandler *errh)
{
    Switch *sw = (Switch *)e;
    int sw_output;
    if (!IntArg().parse(s, sw_output))
	return errh->error("Switch output must be integer");
    if (sw_output >= sw->noutputs())
	sw_output = -1;
    sw->_output = sw_output;
    return 0;
}

void
Switch::add_handlers()
{
    add_read_handler("switch", read_param, 0);
    add_write_handler("switch", write_param, 0, Handler::h_nonexclusive);
    add_read_handler("config", read_param, 0);
    set_handler_flags("config", 0, Handler::CALM);
}

int
Switch::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_SET_SWITCH) {
    int32_t *val = reinterpret_cast<int32_t *>(data);
    if (*val >= noutputs())
      return -EINVAL;
    _output = *val;
    return 0;

  } else if (command == CLICK_LLRPC_GET_SWITCH) {
    int32_t *val = reinterpret_cast<int32_t *>(data);
    *val = _output;
    return 0;

  } else
    return Element::llrpc(command, data);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Switch)
ELEMENT_MT_SAFE(Switch)
