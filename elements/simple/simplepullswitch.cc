/*
 * simplepullswitch.{cc,hh} -- element routes packets from one input of several
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
#include "simplepullswitch.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/llrpc.h>
CLICK_DECLS

SimplePullSwitch::SimplePullSwitch()
{
}

SimplePullSwitch::~SimplePullSwitch()
{
}

int
SimplePullSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int input = 0;
    if (Args(conf, this, errh).read_p("INPUT", input).complete() < 0)
	return -1;
    set_input(input);
    return 0;
}

void
SimplePullSwitch::set_input(int input)
{
    _input = (input < 0 || input >= ninputs() ? -1 : input);
}

Packet *
SimplePullSwitch::pull(int)
{
    if (_input < 0)
	return 0;
    else
	return input(_input).pull();
}

int
SimplePullSwitch::write_param(const String &s, Element *e, void *, ErrorHandler *errh)
{
    SimplePullSwitch *sw = static_cast<SimplePullSwitch *>(e);
    int input;
    if (!IntArg().parse(s, input))
	return errh->error("syntax error");
    sw->set_input(input);
    return 0;
}

void
SimplePullSwitch::add_handlers()
{
    add_data_handlers("switch", Handler::OP_READ, &_input);
    add_write_handler("switch", write_param);
    add_data_handlers("config", Handler::OP_READ, &_input);
    set_handler_flags("config", 0, Handler::CALM);
}

int
SimplePullSwitch::llrpc(unsigned command, void *data)
{
    if (command == CLICK_LLRPC_SET_SWITCH) {
	int32_t *val = reinterpret_cast<int32_t *>(data);
	set_input(*val);
	return 0;

    } else if (command == CLICK_LLRPC_GET_SWITCH) {
	int32_t *val = reinterpret_cast<int32_t *>(data);
	*val = _input;
	return 0;

    } else
	return Element::llrpc(command, data);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimplePullSwitch)
ELEMENT_MT_SAFE(SimplePullSwitch)
